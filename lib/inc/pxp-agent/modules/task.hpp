#ifndef SRC_MODULES_TASK_H_
#define SRC_MODULES_TASK_H_

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

class Task : public PXPAgent::Util::BoltModule, public PXPAgent::Util::Purgeable {
  public:
    Task(const boost::filesystem::path& exec_prefix,
         const std::string& task_cache_dir,
         const std::string& task_cache_dir_purge_ttl,
         const std::vector<std::string>& master_uris,
         const std::string& ca,
         const std::string& crt,
         const std::string& key,
         const std::string& proxy,
         uint32_t task_download_connect_timeout_s,
         uint32_t task_download_timeout_s,
         std::shared_ptr<ResultsStorage> storage);


    /// Utility to purge tasks from the task_cache_dir that have surpassed the ttl.
    /// If a purge_callback is not specified, the boost filesystem's remove_all() will be used.
    /// Returns number of directories purged.
    unsigned int purge(
        const std::string& ttl,
        std::vector<std::string> ongoing_transactions,
        std::function<void(const std::string& dir_path)> purge_callback = nullptr) override;

    std::set<std::string> const& features() const;

  private:
    std::string task_cache_dir_;
    PCPClient::Util::mutex task_cache_dir_mutex_;

    boost::filesystem::path exec_prefix_;

    std::vector<std::string> master_uris_;

    uint32_t task_download_connect_timeout_, task_download_timeout_;

    std::set<std::string> features_;

    leatherman::curl::client client_;

    boost::filesystem::path downloadMultiFile(std::vector<leatherman::json_container::JsonContainer> const& files,
        std::set<std::string> const& download_set,
        boost::filesystem::path const& spool_dir);

    leatherman::json_container::JsonContainer selectLibFile(std::vector<leatherman::json_container::JsonContainer> const& files,
        std::string const& file_name);

    Util::CommandObject buildCommandObject(const ActionRequest& request) override;
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_TASK_H_
