#ifndef SRC_MODULES_DOWNLOAD_FILE_H_
#define SRC_MODULES_DOWNLOAD_FILE_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/module_cache_dir.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/util/purgeable.hpp>
#include <pxp-agent/util/bolt_module.hpp>

#include <cpp-pcp-client/util/thread.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/curl/client.hpp>
#include <set>

namespace PXPAgent {
namespace Modules {

  class DownloadFile : public PXPAgent::Util::BoltModule, public PXPAgent::Util::Purgeable {
    public:
      DownloadFile(const std::vector<std::string>& master_uris,
                  const std::string& ca,
                  const std::string& crt,
                  const std::string& key,
                  const std::string& proxy,
                  uint32_t download_connect_timeout,
                  uint32_t download_timeout,
                  std::shared_ptr<ModuleCacheDir> module_cache_dir,
                  std::shared_ptr<ResultsStorage> storage);

      /// Utility to purge files from the cache_dir that have surpassed the ttl.
      /// If a purge_callback is not specified, the boost filesystem's remove_all() will be used.
      /// Returns number of directories purged.
      unsigned int purge(
          const std::string& ttl,
          std::vector<std::string> ongoing_transactions,
          std::function<void(const std::string& dir_path)> purge_callback = nullptr) override;

    private:
      boost::filesystem::path exec_prefix_;

      std::vector<std::string> master_uris_;

      uint32_t file_download_connect_timeout_, file_download_timeout_;

      leatherman::curl::client client_;

      // callAction is normally implemented in the BoltModule base class. However:
      // DownloadFile will not execute any external processes, so it does not need the
      // overhead from callAction to parse blocking/non-blocking and call extrenal
      // processes.
      //
      // DownloadFile re-implements callAction to provide the functionality to download
      // files.
      ActionResponse callAction(const ActionRequest& request) override;

      // Since DownloadFile overrides callAction there's no reason to define
      // buildCommandObject (since it will never be called)
      Util::CommandObject buildCommandObject(const ActionRequest& request) override {
        throw Module::ProcessingError(leatherman::locale::format("DownloadFile module does not implement buildCommandObject!"));
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
