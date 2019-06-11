#include <pxp-agent/modules/download_file.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/util/bolt_module.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/module.hpp>
#include <boost/algorithm/hex.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.module.download_file"
#include <leatherman/logging/logging.hpp>
#include <vector>

namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;
namespace pcp_util = PCPClient::Util;
namespace fs       = boost::filesystem;

namespace PXPAgent {
namespace Modules {

  static const std::string DOWNLOAD_FILE_ACTION { "download" };

  static const std::string DOWNLOAD_FILE_ACTION_INPUT_SCHEMA { R"(
  {
    "type": "object",
    "properties": {
      "file": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "filename": {
              "type": "string"
            },
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
            }
          },
          "required": ["filename", "destination", "uri", "sha256"]
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
    BoltModule { std::move(storage), std::move(module_cache_dir) },
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


  // DownloadFile overrides callAction from the base BoltModule class since there's no need to run
  // any commands with DownloadFile. CallAction will simply download the file and return a result
  // based on if the download succeeded or failed.
  ActionResponse DownloadFile::callAction(const ActionRequest& request)
  {
    auto file_params = request.params();
    auto file = file_params.get<lth_jc::JsonContainer>("file");
    auto destination = fs::path(file.get<std::string>("destination"));

    ActionResponse response { ModuleType::Internal, request };
    lth_jc::JsonContainer result;
    try {
      Util::downloadFileFromMaster(master_uris_,
                    file_download_connect_timeout_,
                    file_download_timeout_,
                    client_,
                    module_cache_dir_->createCacheDir(file.get<std::string>("sha256")),
                    destination,
                    file);
      response.output = ActionOutput { EXIT_SUCCESS, "", "" };
    } catch (Module::ProcessingError& e) {
      LOG_ERROR("Failed to download {1}; {2}", destination, e.what());
      response.output = ActionOutput { EXIT_FAILURE, "", lth_loc::format("Failed to download {1}; {2}", destination, e.what()) };
    }
    processOutputAndUpdateMetadata(response);
    return response;
  }

}  // namespace Modules
}  // namespace PXPAgent
