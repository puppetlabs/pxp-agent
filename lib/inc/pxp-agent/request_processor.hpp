#ifndef SRC_AGENT_REQUEST_PROCESSOR_HPP_
#define SRC_AGENT_REQUEST_PROCESSOR_HPP_

#include <pxp-agent/module.hpp>
#include <pxp-agent/thread_container.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/export.h>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <vector>

namespace PXPAgent {

class LIBPXP_AGENT_EXPORT RequestProcessor {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    RequestProcessor() = delete;

    RequestProcessor(std::shared_ptr<PXPConnector> connector_ptr,
                     const Configuration::Agent& agent_configuration);

    /// Execute the specified action.
    ///
    /// In case of blocking action, once it's done, send back to the
    /// requester a blocking response containing the action results.
    /// Propagates possible request errors raised by the action logic.
    /// In case it fails to send the response, no further attempt will
    /// be made.
    ///
    /// In case of non-blocking action, start a task for the specified
    /// action in a separate execution thread.
    /// Once the task has started, send a provisional response to the
    /// requester. In case the request has the notify_outcome field
    /// flagged, the task will send a non-blocking response
    /// containing the action outcome, after the action is done. The
    /// task will also write the action outcome and request metadata
    /// to disk.
    void processRequest(const RequestType& request_type,
                        const PCPClient::ParsedChunks& parsed_chunks);

  private:
    /// Manages the lifecycle of non-blocking action jobs
    ThreadContainer thread_container_;

    /// PXP Connector pointer
    std::shared_ptr<PXPConnector> connector_ptr_;

    /// Where the directories for non-blocking actions results will
    /// be created
    const std::string spool_dir_;

    /// Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    /// Where the configuration files of modules are stored
    const std::string modules_config_dir_;

    /// Modules configuration
    std::map<std::string, lth_jc::JsonContainer> modules_config_;

    /// Throw a RequestProcessor::Error in case of unknown module,
    /// unknown action, or if the requested input parameters entry
    /// does not match the JSON schema defined for the relevant action
    void validateRequestContent(const ActionRequest& request);

    void processBlockingRequest(const ActionRequest& request);

    void processNonBlockingRequest(const ActionRequest& request);

    /// Load the modules configuration files
    void loadModulesConfiguration();

    /// Load the modules from the src/modules directory
    void loadInternalModules();

    /// Load the external modules contained in the specified directory
    void loadExternalModulesFrom(boost::filesystem::path modules_dir_path);

    /// Log the loaded modules
    void logLoadedModules() const;
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_REQUEST_PROCESSOR_HPP_
