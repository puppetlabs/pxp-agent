#ifndef SRC_MODULES_APPLY_H_
#define SRC_MODULES_APPLY_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/util/bolt_module.hpp>
#include <pxp-agent/util/purgeable.hpp>
#include <pxp-agent/module_cache_dir.hpp>

#include <leatherman/locale/locale.hpp>

namespace PXPAgent {
namespace Modules {

class Apply : public PXPAgent::Util::BoltModule, public PXPAgent::Util::Purgeable {
    public:
        Apply(const boost::filesystem::path& exec_prefix,
              const std::vector<std::string>& primary_uris,
              const std::string& ca,
              const std::string& crt,
              const std::string& key,
              const std::string& crl,
              const std::string& proxy,
              std::shared_ptr<ModuleCacheDir> module_cache_dir,
              std::shared_ptr<ResultsStorage> storage);

        Util::CommandObject buildCommandObject(const ActionRequest& request) override;

        /// Utility to purge files from the cache_dir that have surpassed the ttl.
        /// If a purge_callback is not specified, the boost filesystem's remove_all() will be used.
        /// Returns number of directories purged.
        unsigned int purge(
            const std::string& ttl,
            std::vector<std::string> ongoing_transactions,
            std::function<void(const std::string& dir_path)> purge_callback = nullptr) override;

    private:
      std::vector<std::string> primary_uris_;
      std::string ca_;
      std::string crt_;
      std::string key_;
      std::string crl_;
      std::string proxy_;
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_APPLY_H_
