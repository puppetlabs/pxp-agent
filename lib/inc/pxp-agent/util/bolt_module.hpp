#ifndef SRC_UTIL_BOLT_MODULE_HPP_
#define SRC_UTIL_BOLT_MODULE_HPP_

#include <pxp-agent/action_response.hpp>
#include <pxp-agent/module.hpp>
#include <pxp-agent/module_cache_dir.hpp>
#include <pxp-agent/results_storage.hpp>

#include <leatherman/execution/execution.hpp>

#include <boost/filesystem.hpp>

namespace PXPAgent {
namespace Util {

// CommandObject holds collected parameters for leatherman's execution methods
struct CommandObject {
    std::string executable;
    std::vector<std::string> arguments;
    std::map<std::string, std::string> environment;
    std::string input;
    std::function<void(size_t)> pid_callback;
};

// This module is a basis for PXP modules supporting bolt functionality
class BoltModule : public PXPAgent::Module {
    public:
        BoltModule(const boost::filesystem::path &exec_prefix,
                   std::shared_ptr<ResultsStorage> storage,
                   std::shared_ptr<ModuleCacheDir> module_cache_dir)
            : exec_prefix_(exec_prefix),
              storage_(std::move(storage)),
              module_cache_dir_(std::move(module_cache_dir)) {}

        /// Whether the module supports non-blocking / asynchronous requests.
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

    protected:
        boost::filesystem::path exec_prefix_;
        std::shared_ptr<ResultsStorage> storage_;
        std::shared_ptr<ModuleCacheDir> module_cache_dir_;

        // Construct a CommandObject based on an ActionRequest - all inheriting classes must implement this method.
        virtual CommandObject buildCommandObject(const ActionRequest& request) = 0;

        // Execute a CommandObject synchronously
        virtual leatherman::execution::result run_sync(const CommandObject &cmd);

        // Execute a CommandObject asynchronously, spawning a new process
        virtual leatherman::execution::result run(const CommandObject &cmd);

        virtual void callBlockingAction(
                const ActionRequest& request,
                const Util::CommandObject &command,
                ActionResponse &response);

        virtual void callNonBlockingAction(
                const ActionRequest& request,
                const CommandObject &command,
                ActionResponse &response);

        ActionResponse callAction(const ActionRequest& request) override;
};

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_UTIL_BOLT_MODULE_HPP_
