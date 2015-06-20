#ifndef SRC_AGENT_REQUEST_PROCESSOR_HPP_
#define SRC_AGENT_REQUEST_PROCESSOR_HPP_

#include <cthun-agent/module.hpp>
#include <cthun-agent/thread_container.hpp>
#include <cthun-agent/action_request.hpp>
#include <cthun-agent/cthun_connector.hpp>

#include <cthun-client/data_container/data_container.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <vector>

namespace CthunAgent {

class RequestProcessor {
  public:
    RequestProcessor() = delete;

    RequestProcessor(std::shared_ptr<CthunConnector> connector_ptr,
                     const std::string& modules_dir);

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
                        const CthunClient::ParsedChunks& parsed_chunks);

  private:
    /// Manages the lifecycle of non-blocking action jobs
    ThreadContainer thread_container_;

    /// Cthun Connector pointer
    std::shared_ptr<CthunConnector> connector_ptr_;

    /// Where the directories for non-blocking actions results will
    /// be created
    const std::string spool_dir_;

    // Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    /// Throw a request_validation_error in case of unknown module,
    /// unknown action, or if the requested input parameters entry
    /// does not match the JSON schema defined for the relevant action
    void validateRequestContent(const ActionRequest& request);

    void processBlockingRequest(const ActionRequest& request);

    void processNonBlockingRequest(const ActionRequest& request);

    /// Load the modules from the src/modules directory
    void loadInternalModules();

    /// Load the external modules contained in the specified directory
    void loadExternalModulesFrom(boost::filesystem::path modules_dir_path);

    /// Log the loaded modules
    void logLoadedModules() const;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_REQUEST_PROCESSOR_HPP_
