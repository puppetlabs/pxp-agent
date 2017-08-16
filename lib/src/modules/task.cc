#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/system/error_code.hpp>

#include <rapidjson/rapidjson.h>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

#include <openssl/evp.h>
#include <curl/curl.h>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;
namespace alg = boost::algorithm;
namespace boost_error = boost::system::errc;

namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;
namespace lth_curl = leatherman::curl;

static const std::string TASK_RUN_ACTION { "run" };

static const lth_jc::JsonContainer TASK_RUN_ACTION_INPUT_SCHEMA { R"(
{
  "type": "object",
  "properties": {
    "task": {
      "type": "string"
    },
    "files": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "filename": {
            "type": "string"
          },
          "uri": {
            "type": "object",
            "properties": {
              "path": {
                "type": "string"
              },
              "params": {
                 "type": "object"
              }
            },
            "required": ["path", "params"]
          },
          "sha256": {
            "type": "string"
          }
        },
        "required": ["filename", "uri", "sha256"]
      },
      "minItems": 1
    },
    "input": {
      "type": "object"
    },
    "inputFormat": {
      "type": "string"
    }
  },
  "required": ["task", "files", "input"]
}
)" };

#ifdef _WIN32
// The extension is required with lth_exec::execute when using a full path.
static const std::string TASK_WRAPPER_EXECUTABLE { "task_wrapper.exe" };
#else
static const std::string TASK_WRAPPER_EXECUTABLE { "task_wrapper" };
#endif
static const int TASK_WRAPPER_FILE_ERROR_EC { 5 };

// Hard-code interpreters on Windows. On non-Windows, we still rely on permissions and #!
static const std::map<std::string, std::function<TaskCommand(std::string)>> BUILTIN_TASK_INTERPRETERS {
#ifdef _WIN32
    {".rb",  [](std::string filename) { return TaskCommand {
        "ruby", { filename }
    }; }},
    {".pp",  [](std::string filename) { return TaskCommand {
        "puppet", { "apply", filename }
    }; }},
    {".ps1", [](std::string filename) { return TaskCommand {
        "powershell", { "-NoProfile", "-NonInteractive", "-NoLogo", "-ExecutionPolicy", "Bypass", "-File", filename }
    }; }}
#endif
};

Task::Task(const fs::path& exec_prefix,
           const std::string& task_cache_dir,
           const std::vector<std::string>& master_uris,
           const std::string& ca,
           const std::string& crt,
           const std::string& key,
           const std::string& spool_dir) :
    storage_ { spool_dir },
    task_cache_dir_ { task_cache_dir },
    wrapper_executable_ { (exec_prefix / TASK_WRAPPER_EXECUTABLE).string() },
    master_uris_ { master_uris }
{
    module_name = "task";
    actions.push_back(TASK_RUN_ACTION);

    PCPClient::Schema input_schema { TASK_RUN_ACTION, TASK_RUN_ACTION_INPUT_SCHEMA };
    PCPClient::Schema output_schema { TASK_RUN_ACTION };

    input_validator_.registerSchema(input_schema);
    results_validator_.registerSchema(output_schema);

    client_.set_ca_cert(ca);
    client_.set_client_cert(crt, key);
    client_.set_supported_protocols(CURLPROTO_HTTPS);
}

static void addParametersToEnvironment(const lth_jc::JsonContainer &input, std::map<std::string, std::string> &environment)
{
    for (auto& k : input.keys()) {
        auto val = input.type(k) == lth_jc::String ? input.get<std::string>(k) : input.toString(k);
        environment.emplace("PT_"+k, move(val));
    }
}

static TaskCommand getTaskCommand(const fs::path &task_executable)
{
    auto builtin = BUILTIN_TASK_INTERPRETERS.find(task_executable.extension().string());
    if (builtin != BUILTIN_TASK_INTERPRETERS.end()) {
        return builtin->second(task_executable.string());
    }

    return TaskCommand { task_executable.string(), { } };
}

// Converts boost filesystem errors to a task_error object.
static Module::ProcessingError toModuleProcessingError(const fs::filesystem_error& e) {
    auto err_code = e.code();
    if (err_code == boost_error::no_such_file_or_directory) {
        return Module::ProcessingError(lth_loc::format("No such file or directory: {1}", e.path1()));
    } else if (err_code ==  boost_error::file_exists) {
        return Module::ProcessingError(lth_loc::format("A file matching the name of the provided sha already exists"));
    } else {
        return Module::ProcessingError(e.what());
    }
}

#ifndef _WIN32
static const fs::perms NIX_TASK_FILE_PERMS = NIX_DIR_PERMS;
#endif

// Creates the <task_cache_dir>/<sha256> directory for a single
// task, ensuring that its permissions are readable by
// the PXP agent owner/group (for unix OSes), writable for the PXP agent owner,
// and executable by both PXP agent owner and group. Returns the path to this directory.
// Note that the last modified time of the directory is updated, and that this routine
// will not fail if the directory already exists.
static fs::path createCacheDir(const fs::path& task_cache_dir, const std::string& sha256) {
    auto cache_dir = task_cache_dir / sha256;
    fs::create_directory(cache_dir);
    fs::last_write_time(cache_dir, time(nullptr));
#ifndef _WIN32
    fs::permissions(cache_dir, NIX_DIR_PERMS);
#endif
    return cache_dir;
}

// Computes the sha256 of the file denoted by path. Assumes that
// the file designated by "path" exists.
static std::string calculateSha256(const std::string& path) {
    const std::streamsize CHUNK_SIZE = 2 << 14;
    char buffer[CHUNK_SIZE];
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    auto md = EVP_sha256();
    auto mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, nullptr);
    boost::nowide::ifstream ifs(path);
    while (ifs.read(buffer, CHUNK_SIZE)) {
      EVP_DigestUpdate(mdctx, buffer, CHUNK_SIZE);
    }
    if (!ifs.eof()) {
      throw Module::ProcessingError(lth_loc::format("Error while reading {1}", path));
    }
    EVP_DigestUpdate(mdctx, buffer, ifs.gcount());
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_destroy(mdctx);

    char md_value_ascii[2*EVP_MAX_MD_SIZE+1] = {'\0'};
    constexpr size_t hexlen = 3;
    for (unsigned int i = 0; i < md_len; ++i) {
      snprintf(md_value_ascii+2*i, hexlen, "%02x", md_value[i]);
    }
    return std::string(md_value_ascii);
}

static std::string createUrlEndpoint(const lth_jc::JsonContainer& uri) {
    std::string url = uri.get<std::string>("path");
    auto params = uri.getWithDefault<lth_jc::JsonContainer>("params", lth_jc::JsonContainer());
    if (params.empty()) {
        return url;
    }
    auto curl_handle = lth_curl::curl_handle();
    url += "?";
    for (auto& key : params.keys()) {
        auto escaped_key = std::string(
           lth_curl::curl_escaped_string(curl_handle, key));
        auto escaped_val = std::string(
           lth_curl::curl_escaped_string(curl_handle, params.get<std::string>(key)));
        url += escaped_key + "=" + escaped_val + "&";
    }
    return url;
}

// Downloads the file at the specified url into the provided path. If something goes
// wrong during the download, a PXP-error should be returned. Note that the provided
// "file_path" argument is a temporary file, call it "tempA". Leatherman.curl during
// the download method will create another temporary file, call it "tempB", to save
// the downloaded task file's contents in chunks before renaming it to "tempA." The
// rationale behind this solution is that:
//    (1) After download, we still need to check "tempA" to ensure that its sha matches
//    the provided sha. So the downloaded task is not quite a "valid" task after this
//    method is called; it's still temporary.
//
//    (2) It somewhat simplifies error handling if multiple threads try to download
//    the same task file.
// The downloaded task file's permissions will be set to rwx for user and rx for
// group for non-Windows OSes.
static void downloadTaskFile(const std::vector<std::string>& master_uris,
                             lth_curl::client& client,
                             const fs::path& file_path,
                             const lth_jc::JsonContainer& uri) {
    auto url = master_uris[0] + createUrlEndpoint(uri);
    lth_curl::request req(url);
    // timeout from connection after one minute, can configure
    req.connection_timeout(60000);
#ifndef _WIN32
    client.download_file(req, file_path.string(), NIX_TASK_FILE_PERMS);
#else
    client.download_file(req, file_path.string());
#endif
}

// This method does the following. If the file matching the "filename" field of the
// file_obj JSON does not exist OR if its hash does not match the sha value in the
// "sha256" field of file_obj, then:
//    (1) The file is downloaded using Leatherman.curl from the first entry in
//    master-uris to a temporary directory. If this download fails, a PXP error
//    is thrown.
//
//    (2) If the downloaded file's sha does not match the provided sha, then a PXP
//        error is returned.
//
//    (3) If (1) and (2) both succeed, then the downloaded file is atomically
//        renamed to cache_dir/<filename>
static fs::path updateTaskFile(const std::vector<std::string>& master_uris,
                               lth_curl::client& client,
                               const fs::path& cache_dir,
                               const lth_jc::JsonContainer& file) {
    auto filename = file.get<std::string>("filename");
    auto sha256 = file.get<std::string>("sha256");
    auto filepath = cache_dir / filename;

    if (fs::exists(filepath) && sha256 == calculateSha256(filepath.string())) {
#ifndef _WIN32
        fs::permissions(filepath, NIX_TASK_FILE_PERMS);
#endif
        return filepath;
    }

    if (master_uris.empty()) {
        throw Module::ProcessingError(lth_loc::format("Cannot download task. No master-uris were provided"));
    }
    auto tempname = cache_dir / fs::unique_path("temp_task_%%%%-%%%%-%%%%-%%%%");
    downloadTaskFile(master_uris, client, tempname, file.get<lth_jc::JsonContainer>("uri"));
    if (sha256 != calculateSha256(tempname.string())) {
      fs::remove(tempname);
      throw Module::ProcessingError(lth_loc::format("The downloaded {1}'s sha differs from the provided sha", filename));
    }
    fs::rename(tempname, filepath);
    return filepath;
}

// Verify (this includes checking the SHA256 checksums) that *all* task files are present
// in the task cache downloading them if necessary.
// Return the full path of the cached version of the first file from the list (which
// is assumed to be the task executable).
static fs::path getCachedTaskFile(const fs::path& task_cache_dir,
                                  const std::vector<std::string>& master_uris,
                                  lth_curl::client& client,
                                  const std::vector<lth_jc::JsonContainer> &files) {
    if (files.empty()) {
        throw Module::ProcessingError {
            lth_loc::format("at least one file must be specified for a task") };
    }
    auto file = files[0];
    LOG_DEBUG("Verifying task file based on {1}", file.toString());

    try {
        auto cache_dir = createCacheDir(task_cache_dir, file.get<std::string>("sha256"));
        return updateTaskFile(master_uris, client, cache_dir, file);
    } catch (lth_curl::http_file_download_exception& e) {
        throw Module::ProcessingError(lth_loc::format("Downloading the task file {1} failed. Reason: {2}", file.get<std::string>("filename"), e.what()));
    } catch (fs::filesystem_error& e) {
        throw toModuleProcessingError(e);
    }
}

void Task::callBlockingAction(
    const ActionRequest& request,
    const TaskCommand &command,
    const std::map<std::string, std::string> &environment,
    const std::string &input,
    ActionResponse &response
) {
    auto exec = lth_exec::execute(
        command.executable,
        command.arguments,
        input,
        environment,
        0,  // timeout
        leatherman::util::option_set<lth_exec::execution_options> {
            lth_exec::execution_options::thread_safe,
            lth_exec::execution_options::merge_environment,
            lth_exec::execution_options::inherit_locale });  // options

    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    processOutputAndUpdateMetadata(response);
}

void Task::callNonBlockingAction(
    const ActionRequest& request,
    const TaskCommand &command,
    const std::map<std::string, std::string> &environment,
    const std::string &input,
    ActionResponse &response
) {
    const fs::path &results_dir = request.resultsDir();
    lth_jc::JsonContainer wrapper_input;

    wrapper_input.set<std::string>("executable", command.executable);
    wrapper_input.set<std::vector<std::string>>("arguments", command.arguments);
    wrapper_input.set<std::string>("input", input);
    wrapper_input.set<std::string>("stdout", (results_dir / "stdout").string());
    wrapper_input.set<std::string>("stderr", (results_dir / "stderr").string());
    wrapper_input.set<std::string>("exitcode", (results_dir / "exitcode").string());

    auto exec = lth_exec::execute(
        wrapper_executable_,
        {},
        wrapper_input.toString(),
        environment,
        [results_dir](size_t pid) {
            auto pid_file = (results_dir / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                           NIX_FILE_PERMS, std::ios::binary);
        },  // pid callback
        0,  // timeout
        leatherman::util::option_set<lth_exec::execution_options> {
            lth_exec::execution_options::thread_safe,
            lth_exec::execution_options::create_detached_process,
            lth_exec::execution_options::merge_environment,
            lth_exec::execution_options::inherit_locale });  // options

    if (exec.exit_code == TASK_WRAPPER_FILE_ERROR_EC) {
        // This is unexpected. The output of the task will not be
        // available for future transaction status requests; we cannot
        // provide a reliable ActionResponse.
        std::string empty_label { lth_loc::translate("(empty)") };
        LOG_WARNING("The task wrapper process failed to write output on file for the {1}; "
                    "stdout: {2}; stderr: {3}",
                    request.prettyLabel(),
                    (exec.output.empty() ? empty_label : exec.output),
                    (exec.error.empty() ? empty_label : exec.error));
        throw Module::ProcessingError {
            lth_loc::translate("failed to write output on file") };
    }

    // Stdout / stderr output should be on file; read it
    response.output = storage_.getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
}

ActionResponse Task::callAction(const ActionRequest& request)
{
    auto task_execution_params = request.params();
    auto task_input_format = task_execution_params.includes("inputFormat") ?
        task_execution_params.get<std::string>("inputFormat") : std::string{""};
    std::map<std::string, std::string> task_environment;
    std::string task_input;

    bool has_input = false;
    if (task_input_format.empty() || task_input_format == "stdin:json") {
        task_input = task_execution_params.get<lth_jc::JsonContainer>("input").toString();
        has_input = true;
    }

    if (task_input_format.empty() || task_input_format == "environment") {
        addParametersToEnvironment(task_execution_params.get<lth_jc::JsonContainer>("input"), task_environment);
        has_input = true;
    }

    if (!has_input) {
        throw Module::ProcessingError {
            lth_loc::format("unsupported task input format: {1}", task_input_format) };
    }

    auto task_command = getTaskCommand(
        getCachedTaskFile(task_cache_dir_,
                          master_uris_,
                          client_,
                          task_execution_params.get<std::vector<lth_jc::JsonContainer>>("files")));
    ActionResponse response { ModuleType::Internal, request };

    if (request.type() == RequestType::Blocking) {
        callBlockingAction(request, task_command, task_environment, task_input, response);
    } else {
        // Guaranteed by Configuration
        assert(!request.resultsDir().empty());
        callNonBlockingAction(request, task_command, task_environment, task_input, response);
    }

    return response;
}

static bool isValidUTF8(std::string &s)
{
    rapidjson::StringStream source(s.data());
    rapidjson::InsituStringStream target(&s[0]);

    target.PutBegin();
    while (source.Tell() < s.size()) {
        if (!rapidjson::UTF8<char>::Validate(source, target)) {
            return false;
        }
    }
    return true;
}

void Task::processOutputAndUpdateMetadata(ActionResponse& response)
{
    if (response.output.std_out.empty()) {
        LOG_TRACE("Obtained no results on stdout for the {1}",
                  response.prettyRequestLabel());
    } else {
        LOG_TRACE("Results on stdout for the {1}: {2}",
                  response.prettyRequestLabel(), response.output.std_out);
    }

    if (response.output.exitcode != EXIT_SUCCESS) {
        LOG_TRACE("Execution failure (exit code {1}) for the {2}{3}",
                  response.output.exitcode, response.prettyRequestLabel(),
                  (response.output.std_err.empty()
                        ? ""
                        : "; stderr:\n" + response.output.std_err));
    } else if (!response.output.std_err.empty()) {
        LOG_TRACE("Output on stderr for the {1}:\n{2}",
                  response.prettyRequestLabel(), response.output.std_err);
    }

    std::string &output = response.output.std_out;

    if (isValidUTF8(output)) {
        lth_jc::JsonContainer stdout_result;
        stdout_result.set("output", output);

        response.setValidResultsAndEnd(std::move(stdout_result));
    } else {
        LOG_DEBUG("Obtained invalid UTF-8 on stdout for the {1}; stdout:\n{3}",
                  response.prettyRequestLabel(), output);
        std::string execution_error {
            lth_loc::format("The task executed for the {1} returned invalid "
                            "UTF-8 on stdout - stderr:{2}",
                            response.prettyRequestLabel(),
                            (response.output.std_err.empty()
                                ? lth_loc::translate(" (empty)")
                                : "\n" + response.output.std_err)) };
        response.setBadResultsAndEnd(execution_error);
    }
}

}  // namespace Modules
}  // namespace PXPAgent
