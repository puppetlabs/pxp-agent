#ifndef SRC_MODULES_TASK_H_
#define SRC_MODULES_TASK_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/util/purgeable.hpp>

#include <cpp-pcp-client/util/thread.hpp>

#include <leatherman/curl/client.hpp>

namespace PXPAgent {
namespace Modules {

struct TaskCommand {
    std::string executable;
    std::vector<std::string> arguments;
};

class Task : public PXPAgent::Module, public PXPAgent::Util::Purgeable {
  public:
    Task(const boost::filesystem::path& exec_prefix,
         const std::string& task_cache_dir,
         const std::string& task_cache_dir_purge_ttl,
         const std::vector<std::string>& master_uris,
         const std::string& ca,
         const std::string& crt,
         const std::string& key,
         const std::string& crl,
         const std::string& proxy,
         uint32_t task_download_connect_timeout_s,
         uint32_t task_download_timeout_s,
         std::shared_ptr<ResultsStorage> storage);

    /// Whether or not the module supports non-blocking / asynchronous requests.
    bool supportsAsync() override { return true; }

    /// Log information about the output of the performed action
    /// while validating the output is valid UTF-8.
    /// Update the metadata of the ActionResponse instance (the
    /// 'results_are_valid', 'status', and 'execution_error' entries
    /// will be set appropriately; 'end' will be set to the current
    /// time).
    /// This function does not throw a ProcessingError in case of
    /// invalid output on stdout; such failure is instead reported
    /// in the response object's metadata.
    void processOutputAndUpdateMetadata(ActionResponse& response) override;

    /// Utility to purge tasks from the task_cache_dir that have surpassed the ttl.
    /// If a purge_callback is not specified, the boost filesystem's remove_all() will be used.
    /// Returns number of directories purged.
    unsigned int purge(
        const std::string& ttl,
        std::vector<std::string> ongoing_transactions,
        std::function<void(const std::string& dir_path)> purge_callback = nullptr) override;

  private:
    std::shared_ptr<ResultsStorage> storage_;

    std::string task_cache_dir_;
    PCPClient::Util::mutex task_cache_dir_mutex_;

    boost::filesystem::path exec_prefix_;

    std::vector<std::string> master_uris_;

    uint32_t task_download_connect_timeout_, task_download_timeout_;

    leatherman::curl::client client_;

    void callBlockingAction(
        const ActionRequest& request,
        const TaskCommand &command,
        const std::map<std::string, std::string> &environment,
        const std::string &input,
        ActionResponse &response);

    void callNonBlockingAction(
        const ActionRequest& request,
        const TaskCommand &command,
        const std::map<std::string, std::string> &environment,
        const std::string &input,
        ActionResponse &response);

    ActionResponse callAction(const ActionRequest& request) override;
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_TASK_H_
