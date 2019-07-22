#include <pxp-agent/modules/download_file.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/util/bolt_module.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/module.hpp>
#include <boost/algorithm/hex.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.module.download_file"
#include <leatherman/logging/logging.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#include <vector>
#include <string>

namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;
namespace lth_file = leatherman::file_util;
namespace pcp_util = PCPClient::Util;
namespace fs       = boost::filesystem;

namespace PXPAgent {
namespace Modules {

  static const std::string DOWNLOAD_FILE_ACTION { "download" };

  static const std::string DOWNLOAD_FILE_ACTION_INPUT_SCHEMA { R"(
  {
    "type": "object",
    "properties": {
      "files": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "destination": {
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
            },
            "file_type": {
              "type": "string"
            }
          },
          "required": ["destination", "uri", "sha256", "file_type"]
        }
      }
    }
  }
  )" };


  DownloadFile::DownloadFile(const std::vector<std::string>& master_uris,
                             const std::string& ca,
                             const std::string& crt,
                             const std::string& key,
                             const std::string& proxy,
                             uint32_t download_connect_timeout,
                             uint32_t download_timeout,
                             std::shared_ptr<ModuleCacheDir> module_cache_dir,
                             std::shared_ptr<ResultsStorage> storage) :
    BoltModule { "", std::move(storage), std::move(module_cache_dir) },
    Purgeable { module_cache_dir_->purge_ttl_ },
    master_uris_ { master_uris },
    file_download_connect_timeout_ { download_connect_timeout },
    file_download_timeout_ { download_timeout }
  {
    module_name = "download_file";
    actions.push_back(DOWNLOAD_FILE_ACTION);

    PCPClient::Schema input_schema { DOWNLOAD_FILE_ACTION, lth_jc::JsonContainer { DOWNLOAD_FILE_ACTION_INPUT_SCHEMA } };
    PCPClient::Schema output_schema { DOWNLOAD_FILE_ACTION };

    input_validator_.registerSchema(input_schema);
    results_validator_.registerSchema(output_schema);

    client_.set_ca_cert(ca);
    client_.set_client_cert(crt, key);
    client_.set_supported_protocols(CURLPROTO_HTTPS);
    client_.set_proxy(proxy);
  }


  // Write results to results_dir and return them as
  // an ActionOutput struct
  static ActionOutput write_download_results(const fs::path& results_dir,
                                             const int exit_code,
                                             const std::string& std_out,
                                             const std::string& std_err)
  {
    lth_file::atomic_write_to_file(std::to_string(exit_code) + "\n",
                                   (results_dir / "exitcode").string(),
                                   NIX_FILE_PERMS,
                                   std::ios::binary);
    lth_file::atomic_write_to_file(std_out,
                                   (results_dir / "stdout").string(),
                                   NIX_FILE_PERMS,
                                   std::ios::binary);
    lth_file::atomic_write_to_file(std_err,
                                   (results_dir / "stderr").string(),
                                   NIX_FILE_PERMS,
                                   std::ios::binary);
    return ActionOutput { exit_code, std_out, std_err };
  }

  ActionResponse DownloadFile::failure_response(const ActionRequest& request,
                                      const fs::path& results_dir,
                                      const std::string& message)
  {
    LOG_ERROR(message);
    ActionResponse fail_response { ModuleType::Internal, request };
    processOutputAndUpdateMetadata(fail_response);
    fail_response.output = write_download_results(results_dir, EXIT_FAILURE, "", message);
    return fail_response;
  }

  // DownloadFile overrides callAction from the base BoltModule class since there's no need to run
  // any commands with DownloadFile. CallAction will simply download the file and return a result
  // based on if the download succeeded or failed.
  ActionResponse DownloadFile::callAction(const ActionRequest& request)
  {
    auto file_params = request.params();
    auto files = file_params.get<std::vector<lth_jc::JsonContainer>>("files");
    const fs::path& results_dir = request.resultsDir();

    ActionResponse response { ModuleType::Internal, request };
    lth_jc::JsonContainer result;
    for (auto this_file : files) {
      auto destination = fs::path(this_file.get<std::string>("destination"));
      auto file_type = this_file.get<std::string>("file_type");
      if (file_type == "file") {
        try {
          Util::downloadFileFromMaster(master_uris_,
                        file_download_connect_timeout_,
                        file_download_timeout_,
                        client_,
                        module_cache_dir_->createCacheDir(this_file.get<std::string>("sha256")),
                        destination,
                        this_file);
        } catch (Module::ProcessingError& e) {
          return failure_response(request, results_dir, lth_loc::format("Failed to download {1}; {2}", destination, e.what()));
        }
      } else {
          return failure_response(request, results_dir, lth_loc::format("Not a valid file type! {1}", file_type));
      }
    }
    response.output = write_download_results(results_dir, EXIT_SUCCESS, "downloaded", "");
    processOutputAndUpdateMetadata(response);
    return response;
  }


  unsigned int DownloadFile::purge(
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
