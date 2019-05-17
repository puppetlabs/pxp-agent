#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>
#include <pxp-agent/util/utf8.hpp>

#include <cpp-pcp-client/util/chrono.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/system/error_code.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

#include <openssl/evp.h>
#include <curl/curl.h>

#include <tuple>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;
namespace alg = boost::algorithm;
namespace boost_error = boost::system::errc;
namespace pcp_util = PCPClient::Util;

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
    "metadata": {
      "type": "object",
      "properties": {
        "input_method": {
          "type": "string"
        },
        "implementations": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "name": {
                "type": "string"
              },
              "requirements": {
                "type": "array",
                "items": {
                  "type": "string"
                }
              },
              "files": {
                "type": "array",
                "items": {
                  "type": "string"
                }
              },
              "input_method": {
                "type": "string"
              }
            }
          }
        },
        "files": {
          "type": "array",
          "items": {
            "type": "string"
          }
        }
      }
    },
    "features": {
      "type": "array",
      "items": {
        "type": "string"
      }
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
           const std::string& task_cache_dir_purge_ttl,
           const std::vector<std::string>& master_uris,
           const std::string& ca,
           const std::string& crt,
           const std::string& key,
           const std::string& proxy,
           uint32_t task_download_connect_timeout,
           uint32_t task_download_timeout,
           std::shared_ptr<ResultsStorage> storage) :
    Purgeable { task_cache_dir_purge_ttl },
    storage_ { std::move(storage) },
    task_cache_dir_ { task_cache_dir },
    exec_prefix_ { exec_prefix },
    master_uris_ { master_uris },
    task_download_connect_timeout_ { task_download_connect_timeout },
    task_download_timeout_ { task_download_timeout },
#ifdef _WIN32
    features_ { "puppet-agent", "powershell" }
#else
    features_ { "puppet-agent", "shell" }
#endif
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
    client_.set_proxy(proxy);
}

std::set<std::string> const& Task::features() const
{
    return features_;
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

static void createDir(const fs::path& dir) {
    fs::create_directories(dir);
    fs::permissions(dir, NIX_DIR_PERMS);
}

// Creates the <task_cache_dir>/<sha256> directory (and parent dirs) for a single
// task, ensuring that its permissions are readable by
// the PXP agent owner/group (for unix OSes), writable for the PXP agent owner,
// and executable by both PXP agent owner and group. Returns the path to this directory.
// Note that the last modified time of the directory is updated, and that this routine
// will not fail if the directory already exists.
static fs::path createCacheDir(const fs::path& task_cache_dir, const std::string& sha256) {
    auto cache_dir = task_cache_dir / sha256;
    createDir(cache_dir);
    fs::last_write_time(cache_dir, time(nullptr));
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
        boost::nowide::ifstream ifs(path, std::ios::binary);

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
    // Remove trailing ampersand (&)
    url.pop_back();
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
                                                      uint32_t connect_timeout_s,
                                                      uint32_t timeout_s,
                                                      lth_curl::client& client,
                                                      const fs::path& file_path,
                                                      const lth_jc::JsonContainer& uri) {
    auto endpoint = createUrlEndpoint(uri);
    std::tuple<bool, std::string> result = std::make_tuple(false, "");
    for (auto& master_uri : master_uris) {
        auto url = master_uri + endpoint;
        lth_curl::request req(url);

        // Request timeouts expect milliseconds.
        req.connection_timeout(connect_timeout_s*1000);
        req.timeout(timeout_s*1000);

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
                               uint32_t connect_timeout,
                               uint32_t timeout,
                               lth_curl::client& client,
                               const fs::path& cache_dir,
                               const lth_jc::JsonContainer& file) {
    auto filename = fs::path(file.get<std::string>("filename")).filename();
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
    auto download_result = downloadTaskFile(master_uris, connect_timeout, timeout, client, tempname, file.get<lth_jc::JsonContainer>("uri"));
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

// Verify (this includes checking the SHA256 checksums) that task file is present
// in the task cache downloading it if necessary.
// Return the full path of the cached version of the file.
static fs::path getCachedTaskFile(const fs::path& task_cache_dir,
                                  PCPClient::Util::mutex& task_cache_dir_mutex,
                                  const std::vector<std::string>& master_uris,
                                  uint32_t connect_timeout,
                                  uint32_t timeout,
                                  lth_curl::client& client,
                                  lth_jc::JsonContainer& file) {
    LOG_DEBUG("Verifying task file based on {1}", file.toString());

    try {
        auto cache_dir = [&]() {
            pcp_util::lock_guard<pcp_util::mutex> the_lock { task_cache_dir_mutex };
            return createCacheDir(task_cache_dir, file.get<std::string>("sha256"));
        }();
        return updateTaskFile(master_uris, connect_timeout, timeout, client, cache_dir, file);
    } catch (fs::filesystem_error& e) {
        throw toModuleProcessingError(e);
    }
}

// build a spool dir for the supporting library files requested by a multifile task
static fs::path createInstallDir(const fs::path& spool_dir, const std::set<std::string>& download_set) {
    auto install_dir = spool_dir / fs::unique_path("temp_task_%%%%-%%%%-%%%%-%%%%");
    createDir(install_dir);

    // this should generate a unique collection of directory paths to append to install_dir
    // for example:
    // if files had the paths: /one/two/three.txt, /one/two/one.txt, /one/two.txt
    // dir_structure set would contain: /one/two /one
    std::set<fs::path> dir_structure;
    for (auto f_name : download_set) {
        dir_structure.insert(fs::path(f_name).parent_path());
    }

    // now create the directories to copy the required files into
    for (auto dir_path : dir_structure) {
        fs::path tmp = install_dir;
        for (const fs::path &dir : dir_path) {
            tmp /= dir;
            createDir(tmp);
        }
    }

    return install_dir;
}

// Get a unique list of "lib" files that the task has specified in metadata
// this involves enumerating files when directories are requested
std::set<std::string> getMultiFiles(std::vector<std::string> const& meta_files, std::vector<std::string>  const& impl_files, std::vector<lth_jc::JsonContainer> const& files) {
    // build set of unique files to download and exclude the selected task executable
    std::set<std::string> download_set(meta_files.begin(), meta_files.end());
    download_set.insert(impl_files.begin(), impl_files.end());
    std::vector<std::string> directories;

    // expand files from directories specified by trailing '/'
    // get list of directories
    for (auto f_name : download_set) {
        if (f_name.back() == '/') {
            directories.push_back(f_name);
        }
    }
    // replace directories with it's child files
    for (auto f_name : directories) {
      download_set.erase(f_name);
      for (auto f : files) {
          if (f.get<std::string>("filename").substr(0, f_name.size()) == f_name) {
              download_set.insert(f.get<std::string>("filename"));
          }
      }
    }

    return download_set;
}

// iterate over unique set of filenames to support task
// copy file from cache to install_dir
fs::path Task::downloadMultiFile(std::vector<lth_jc::JsonContainer> const& files,
                                 std::set<std::string> const& download_set,
                                 fs::path const& spool_dir)
{
    fs::file_status s = fs::status(spool_dir);
    fs::path spool;
    if (fs::is_directory(s)) {
      spool = spool_dir;
    } else {
      spool = task_cache_dir_;
    }
    auto install_dir = createInstallDir(spool, download_set);
    for (auto file_name : download_set) {
        // get file object info based on name
        auto file_object = selectLibFile(files, file_name);

        // get file from cache, download if necessary
        auto lib_file = getCachedTaskFile(task_cache_dir_,
                                       task_cache_dir_mutex_,
                                       master_uris_,
                                       task_download_connect_timeout_,
                                       task_download_timeout_,
                                       client_,
                                       file_object);

        // copy to expected location in install_dir
        fs::copy_file(lib_file, install_dir / fs::path(file_name));
    }

    return install_dir;
}

lth_jc::JsonContainer Task::selectLibFile(std::vector<lth_jc::JsonContainer> const& files,
                                            std::string const& file_name)
{
    auto file = std::find_if(files.cbegin(), files.cend(),
        [&](lth_jc::JsonContainer const& file) {
            auto filename = file.get<std::string>("filename");
            return filename == file_name;
        });

    if (file == files.cend()) {
        throw Module::ProcessingError {
            lth_loc::format("'{1}' file requested as additional task dependency not found", file_name) };
    }
    return *file;
}

struct Implementation {
    std::string name;
    std::string input_method;
    std::vector<std::string> files;
};

static Implementation selectImplementation(std::vector<lth_jc::JsonContainer> const& implementations,
                                           std::set<std::string> const& features)
{
    if (implementations.empty()) {
        return {};
    }

    // Select first entry in implementations where all requirements are in features.
    // If none, throw an error.
    auto impl = std::find_if(implementations.cbegin(), implementations.cend(),
        [&](lth_jc::JsonContainer const& impl) {
            auto reqs = impl.getWithDefault<std::vector<std::string>>("requirements", {});
            if (reqs.empty()) {
                return true;
            } else {
                return all_of(reqs.cbegin(), reqs.cend(),
                    [&](std::string const& req) { return features.count(req) != 0; });
            }
        });

    if (impl != implementations.cend()) {
        auto input_method = impl->getWithDefault<std::string>("input_method", {});
        auto files = impl->getWithDefault<std::vector<std::string>>("files", {});
        return Implementation { impl->get<std::string>("name"), input_method, files };
    } else {
        auto feature_list = boost::algorithm::join(features, ", ");
        throw Module::ProcessingError {
            lth_loc::format("no implementations match supported features: {1}", feature_list) };
    }
}

static lth_jc::JsonContainer selectTaskFile(std::vector<lth_jc::JsonContainer> const& files,
                                            Implementation const& impl)
{
    if (files.empty()) {
        throw Module::ProcessingError {
            lth_loc::format("at least one file must be specified for a task") };
    }

    if (impl.name.empty()) {
        return files[0];
    }

    // Select file based on impl.
    auto file = std::find_if(files.cbegin(), files.cend(),
        [&](lth_jc::JsonContainer const& file) {
            auto filename = file.get<std::string>("filename");
            return filename == impl.name;
        });

    if (file == files.cend()) {
        throw Module::ProcessingError {
            lth_loc::format("'{1}' file requested by implementation not found", impl.name) };
    }
    return *file;
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
    response.output = storage_->getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
}

ActionResponse Task::callAction(const ActionRequest& request)
{
    auto task_execution_params = request.params();
    auto task_metadata = task_execution_params.getWithDefault<lth_jc::JsonContainer>("metadata", task_execution_params);
    auto task_name = task_execution_params.get<std::string>("task");

    std::set<std::string> feats = features();
    auto extra_feats = task_metadata.getWithDefault<std::vector<std::string>>("features", {});
    feats.insert(extra_feats.begin(), extra_feats.end());
    LOG_DEBUG("Running task {1} with features: {2}", task_name, boost::algorithm::join(feats, ", "));

    auto implementations = task_metadata.getWithDefault<std::vector<lth_jc::JsonContainer>>("implementations", {});
    auto implementation = selectImplementation(implementations, feats);

    if (implementation.input_method.empty() && task_metadata.includes("input_method")) {
        implementation.input_method = task_metadata.get<std::string>("input_method");
    }

    static std::set<std::string> input_methods{{"stdin", "environment", "powershell"}};
    if (!implementation.input_method.empty() && input_methods.count(implementation.input_method) == 0) {
        throw Module::ProcessingError {
            lth_loc::format("unsupported task input method: {1}", implementation.input_method) };
    }

    auto files = task_execution_params.get<std::vector<lth_jc::JsonContainer>>("files");
    auto file = selectTaskFile(files, implementation);

    auto task_file = getCachedTaskFile(task_cache_dir_,
                                       task_cache_dir_mutex_,
                                       master_uris_,
                                       task_download_connect_timeout_,
                                       task_download_timeout_,
                                       client_,
                                       file);

    // Use powershell input method by default if task uses .ps1 extension.
    if (implementation.input_method.empty() && task_file.extension().string() == ".ps1") {
        implementation.input_method = "powershell";
    }

    auto task_params = task_execution_params.get<lth_jc::JsonContainer>("input");

    task_params.set<std::string>("_task", task_name);

    std::vector<std::string> meta_files;
    // Only get files array from top level of metatdata instead of puppetserver supplied file data
    try {
         meta_files = task_metadata.getWithDefault<std::vector<std::string>>("files", {});
    } catch (const leatherman::json_container::data_type_error& e) {}

    auto lib_files = getMultiFiles(meta_files, implementation.files, files);

    if (lib_files.size() > 0) {
      auto install_dir = downloadMultiFile(files, lib_files, request.resultsDir());
      auto module = task_name.substr(0, task_name.find(':'));
      createDir(install_dir / module);
      createDir(install_dir / module / "tasks");
      auto task_dest = install_dir / module / "tasks" / task_file.filename();
      fs::copy_file(task_file, task_dest);
      task_file = task_dest;

      LOG_DEBUG("Multi file task _installdir: '{1}'", install_dir.string());
      task_params.set<std::string>("_installdir", install_dir.string());
    }

    std::map<std::string, std::string> task_environment;
    std::string task_input;
    TaskCommand task_command;

    if (implementation.input_method == "powershell") {
        // Run using the powershell shim
        task_command = getTaskCommand(exec_prefix_ / "PowershellShim.ps1");
        task_command.arguments.push_back(task_file.string());
        // Pass input on stdin ($input)
        task_input = task_params.toString();
    } else {
        if (implementation.input_method.empty() || implementation.input_method == "stdin") {
            task_input = task_params.toString();
        }

        if (implementation.input_method.empty() || implementation.input_method == "environment") {
            addParametersToEnvironment(task_params, task_environment);
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

    if (Util::isValidUTF8(output)) {
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

unsigned int Task::purge(
    const std::string& ttl,
    std::vector<std::string> ongoing_transactions,
    std::function<void(const std::string& dir_path)> purge_callback)
{
    unsigned int num_purged_dirs { 0 };
    Timestamp ts { ttl };
    if (purge_callback == nullptr)
        purge_callback = &Purgeable::defaultDirPurgeCallback;

    LOG_INFO("About to purge cached tasks from '{1}'; TTL = {2}",
             task_cache_dir_, ttl);

    lth_file::each_subdirectory(
        task_cache_dir_,
        [&](std::string const& s) -> bool {
            fs::path dir_path { s };
            LOG_TRACE("Inspecting '{1}' for purging", s);

            boost::system::error_code ec;
            pcp_util::lock_guard<pcp_util::mutex> the_lock { task_cache_dir_mutex_ };
            auto last_update = fs::last_write_time(dir_path, ec);
            if (ec) {
                LOG_ERROR("Failed to remove '{1}': {2}", s, ec.message());
            } else if (ts.isNewerThan(last_update)) {
                LOG_TRACE("Removing '{1}'", s);

                try {
                    purge_callback(dir_path.string());
                    num_purged_dirs++;
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to remove '{1}': {2}", s, e.what());
                }
            }

            return true;
        });

    LOG_INFO(lth_loc::format_n(
        // LOCALE: info
        "Removed {1} directory from '{2}'",
        "Removed {1} directories from '{2}'",
        num_purged_dirs, num_purged_dirs, task_cache_dir_));
    return num_purged_dirs;
}

}  // namespace Modules
}  // namespace PXPAgent
