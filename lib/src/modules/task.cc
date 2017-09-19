#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/system/error_code.hpp>

#include <rapidjson/rapidjson.h>
#if RAPIDJSON_MAJOR_VERSION > 1 || RAPIDJSON_MAJOR_VERSION == 1 && RAPIDJSON_MINOR_VERSION >= 1
// Header for StringStream was added in rapidjson 1.1 in a backwards incompatible way.
#include <rapidjson/stream.h>
#endif

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

#include <openssl/evp.h>
#include <curl/curl.h>

#include <tuple>
#include <set>

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

static const std::string TASK_RUN_ACTION_INPUT_SCHEMA { R"(
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
    "input_method": {
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
    exec_prefix_ { exec_prefix },
    master_uris_ { master_uris }
{
    module_name = "task";
    actions.push_back(TASK_RUN_ACTION);

    PCPClient::Schema input_schema { TASK_RUN_ACTION, lth_jc::JsonContainer { TASK_RUN_ACTION_INPUT_SCHEMA } };
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

// A define is used here because creating a local variable is subject to static initialization ordering.
#define NIX_TASK_FILE_PERMS NIX_DIR_PERMS

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
    fs::permissions(cache_dir, NIX_DIR_PERMS);
    return cache_dir;
}

// Computes the sha256 of the file denoted by path. Assumes that
// the file designated by "path" exists.
static std::string calculateSha256(const std::string& path) {
    auto mdctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    {
        constexpr std::streamsize CHUNK_SIZE = 0x8000;  // 32 kB
        char buffer[CHUNK_SIZE];
        boost::nowide::ifstream ifs(path);

        while (ifs.read(buffer, CHUNK_SIZE)) {
            EVP_DigestUpdate(mdctx, buffer, CHUNK_SIZE);
        }
        if (!ifs.eof()) {
            EVP_MD_CTX_destroy(mdctx);
            throw Module::ProcessingError(lth_loc::format("Error while reading {1}", path));
        }
        EVP_DigestUpdate(mdctx, buffer, ifs.gcount());
    }

    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_destroy(mdctx);

    std::string md_value_hex;

    md_value_hex.reserve(2*md_len);
    // TODO use boost::algorithm::hex_lower and drop the std::transform below when we upgrade to boost 1.62.0 or newer
    alg::hex(md_value, md_value+md_len, std::back_inserter(md_value_hex));
    std::transform(md_value_hex.begin(), md_value_hex.end(), md_value_hex.begin(), ::tolower);

    return md_value_hex;
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

// Downloads the file at the specified url into the provided path. Note that the provided
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
//
// The method returns a tuple (success, err_msg). success is true if the file was downloaded;
// false otherwise. err_msg contains the most recent http_file_download_exception's error
// message; it is initially empty.
static std::tuple<bool, std::string> downloadTaskFile(const std::vector<std::string>& master_uris,
                                                      lth_curl::client& client,
                                                      const fs::path& file_path,
                                                      const lth_jc::JsonContainer& uri) {
    auto endpoint = createUrlEndpoint(uri);
    std::tuple<bool, std::string> result = std::make_tuple(false, "");
    for (auto& master_uri : master_uris) {
        auto url = master_uri + endpoint;
        lth_curl::request req(url);
        // timeout from connection after one minute, can configure
        req.connection_timeout(60000);
        try {
            lth_curl::response resp;
            client.download_file(req, file_path.string(), resp, NIX_TASK_FILE_PERMS);
            if (resp.status_code() >= 400) {
                throw lth_curl::http_file_download_exception(
                    req,
                    file_path.string(),
                    lth_loc::format("{1} returned a response with HTTP status {2}. Response body: {3}", url, resp.status_code(), resp.body()));
            }
        } catch (lth_curl::http_file_download_exception& e) {
            // Server-side error, do nothing here -- we want to try the next master-uri.
            LOG_WARNING("Downloading the task file from the master-uri '{1}' failed. Reason: {2}", master_uri, e.what());
            std::get<1>(result) = e.what();
        } catch (lth_curl::http_request_exception& e) {
            // For http_curl_setup and http_file_operation exceptions
            throw Module::ProcessingError(lth_loc::format("Downloading the task file failed. Reason: {1}", e.what()));
        }

        if (fs::exists(file_path)) {
            std::get<0>(result) = true;
            return result;
        }
    }

    return result;
}

// This method does the following. If the file matching the "filename" field of the
// file_obj JSON does not exist OR if its hash does not match the sha value in the
// "sha256" field of file_obj, then:
//    (1) The file is downloaded using Leatherman.curl by trying each of the master_uris
//    until one of them succeeds. If this download fails, a PXP error is thrown.
//
//    (2) If the downloaded file's sha does not match the provided sha, then a PXP
//        error is returned. TODO: Now that we are trying all the master_uris for
//        download, should we try another master_uri if the shas do not match?
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
        fs::permissions(filepath, NIX_TASK_FILE_PERMS);
        return filepath;
    }

    if (master_uris.empty()) {
        throw Module::ProcessingError(lth_loc::format("Cannot download task. No master-uris were provided"));
    }

    auto tempname = cache_dir / fs::unique_path("temp_task_%%%%-%%%%-%%%%-%%%%");
    auto download_result = downloadTaskFile(master_uris, client, tempname, file.get<lth_jc::JsonContainer>("uri"));
    if (!std::get<0>(download_result)) {
        throw Module::ProcessingError(lth_loc::format(
              "Downloading the task file {1} failed after trying all the available master-uris. Most recent error message: {2}",
              file.get<std::string>("filename"),
              std::get<1>(download_result)));
    }

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
        (exec_prefix_ / TASK_WRAPPER_EXECUTABLE).string(),
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

    // Stdout / stderr output should be on file; read it
    response.output = storage_.getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
}

ActionResponse Task::callAction(const ActionRequest& request)
{
    auto task_execution_params = request.params();
    auto task_input_method = task_execution_params.includes("input_method") ?
        task_execution_params.get<std::string>("input_method") : std::string{""};

    static std::set<std::string> input_methods{{"stdin", "environment", "powershell"}};
    if (!task_input_method.empty() && input_methods.count(task_input_method) == 0) {
        throw Module::ProcessingError {
            lth_loc::format("unsupported task input method: {1}", task_input_method) };
    }

    auto task_file = getCachedTaskFile(task_cache_dir_,
                                       master_uris_,
                                       client_,
                                       task_execution_params.get<std::vector<lth_jc::JsonContainer>>("files"));

    // Use powershell input method by default if task uses .ps1 extension.
    if (task_input_method.empty() && task_file.extension().string() == ".ps1") {
        task_input_method = "powershell";
    }

    std::map<std::string, std::string> task_environment;
    std::string task_input;
    TaskCommand task_command;

    if (task_input_method == "powershell") {
        // Run using the powershell shim
        task_command = getTaskCommand(exec_prefix_ / "PowershellShim.ps1");
        task_command.arguments.push_back(task_file.string());
        // Pass input on stdin ($input)
        task_input = task_execution_params.get<lth_jc::JsonContainer>("input").toString();
    } else {
        if (task_input_method.empty() || task_input_method == "stdin") {
            task_input = task_execution_params.get<lth_jc::JsonContainer>("input").toString();
        }

        if (task_input_method.empty() || task_input_method == "environment") {
            addParametersToEnvironment(task_execution_params.get<lth_jc::JsonContainer>("input"), task_environment);
        }

        task_command = getTaskCommand(task_file);
    }

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
        // Return all relevant results: exitcode, stdout, stderr.
        lth_jc::JsonContainer result;
        result.set("exitcode", response.output.exitcode);
        if (!output.empty()) {
            result.set("stdout", output);
        }
        if (!response.output.std_err.empty()) {
            result.set("stderr", response.output.std_err);
        }

        response.setValidResultsAndEnd(std::move(result));
    } else {
        LOG_DEBUG("Obtained invalid UTF-8 on stdout for the {1}; stdout:\n{2}",
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
