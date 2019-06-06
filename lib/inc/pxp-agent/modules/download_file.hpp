#ifndef SRC_MODULES_DOWNLOAD_FILE_H_
#define SRC_MODULES_DOWNLOAD_FILE_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/util/purgeable.hpp>
#include <pxp-agent/util/bolt_module.hpp>

#include <cpp-pcp-client/util/thread.hpp>

#include <leatherman/curl/client.hpp>
#include <set>

namespace PXPAgent {
namespace Modules {

class DownloadFile : public PXPAgent::Util::BoltModule {
  public:
    DownloadFile(const std::string& cache_dir,
                 const std::string& cache_dir_purge_ttl,
                 const std::vector<std::string>& master_uris,
                 const std::string& ca,
                 const std::string& crt,
                 const std::string& key,
                 const std::string& proxy,
                 uint32_t download_connect_timeout,
                 uint32_t download_timeout,
                 std::shared_ptr<ResultsStorage> storage);

  private:
    PCPClient::Util::mutex file_cache_dir_mutex_;

    std::string file_cache_dir_;

    boost::filesystem::path exec_prefix_;

    std::vector<std::string> master_uris_;

    uint32_t file_download_connect_timeout_, file_download_timeout_;

    leatherman::curl::client client_;
    ActionResponse callAction(const ActionRequest& request) override;

    // Since DownloadFile overrides callAction here's no reason to define
    // buildCommandObject (since it will never be called)
    Util::CommandObject buildCommandObject(const ActionRequest& request) override {
      return Util::CommandObject {
        "",
        {},
        {},
        "",
        nullptr
      };
    }
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_DOWNLOAD_FILE_H_
