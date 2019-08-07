#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>
#include <pxp-agent/util/utf8.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>

#include <cpp-pcp-client/util/chrono.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
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

Task::Task(const fs::path& exec_prefix,
           const std::vector<std::string>& master_uris,
           const std::string& ca,
           const std::string& crt,
           const std::string& key,
           const std::string& proxy,
           uint32_t task_download_connect_timeout,
           uint32_t task_download_timeout,
           std::shared_ptr<ModuleCacheDir> module_cache_dir,
           std::shared_ptr<ResultsStorage> storage) :
    BoltModule { exec_prefix, std::move(storage), std::move(module_cache_dir) },
    Purgeable { module_cache_dir_->purge_ttl_ },
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

// build a spool dir for the supporting library files requested by a multifile task
static fs::path createInstallDir(const fs::path& spool_dir, const std::set<std::string>& download_set) {
    auto install_dir = spool_dir / fs::unique_path("temp_task_%%%%-%%%%-%%%%-%%%%");
    Util::createDir(install_dir);

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
            Util::createDir(tmp);
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
      spool = module_cache_dir_->cache_dir_;
    }
    auto install_dir = createInstallDir(spool, download_set);
    for (auto file_name : download_set) {
        // get file object info based on name
        auto file_object = selectLibFile(files, file_name);

        // get file from cache, download if necessary
        auto lib_file = Util::getCachedFile(master_uris_,
                                          task_download_connect_timeout_,
                                          task_download_timeout_,
                                          client_,
                                          module_cache_dir_->createCacheDir(file_object.get<std::string>("sha256")),
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

    auto task_file = Util::getCachedFile(master_uris_,
                                       task_download_connect_timeout_,
                                       task_download_timeout_,
                                       client_,
                                       module_cache_dir_->createCacheDir(file.get<std::string>("sha256")),
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
        Util::createDir(install_dir / module);
        Util::createDir(install_dir / module / "tasks");
        auto task_dest = install_dir / module / "tasks" / task_file.filename();
        fs::copy_file(task_file, task_dest);
        task_file = task_dest;

        LOG_DEBUG("Multi file task _installdir: '{1}'", install_dir.string());
        task_params.set<std::string>("_installdir", install_dir.string());
    }

    // Build a command to run
    Util::CommandObject task_command { "", {}, {}, "", nullptr };

    auto task_file_path = fs::path { task_file };

    if (implementation.input_method == "powershell") {
        // Use the powershell shim as the "task file":
        Util::findExecutableAndArguments(exec_prefix_ / "PowershellShim.ps1", task_command);
        // Pass the original task file to the shim:
        task_command.arguments.push_back(task_file.string());
    } else {
        Util::findExecutableAndArguments(task_file, task_command);
    }

    if (implementation.input_method == "powershell" ||
        (implementation.input_method.empty() || implementation.input_method == "stdin")) {
        task_command.input = task_params.toString();
    }

    // Set the environment variables ($PT_*) for the task parameters
    if (implementation.input_method.empty() || implementation.input_method == "environment") {
        addParametersToEnvironment(task_params, task_command.environment);
    }

    return invokeCommand(request, task_command);
}

unsigned int Task::purge(
    const std::string& ttl,
    std::vector<std::string> ongoing_transactions,
    std::function<void(const std::string& dir_path)> purge_callback)
{
    if (purge_callback == nullptr)
      purge_callback = &Purgeable::defaultDirPurgeCallback;
    return module_cache_dir_->purgeCache(ttl, ongoing_transactions, purge_callback);
}

}  // namespace Modules
}  // namespace PXPAgent
