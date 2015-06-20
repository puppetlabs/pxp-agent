#ifndef SRC_AGENT_REQUEST_PROCESSOR_HPP_
#define SRC_AGENT_REQUEST_PROCESSOR_HPP_

#include <cthun-agent/module.hpp>
#include <cthun-agent/thread_container.hpp>
#include <cthun-agent/action_request.hpp>
#include <cthun-agent/cthun_connector.hpp>

#include <cthun-client/data_container/data_container.hpp>

#include <memory>
#include <string>
#include <vector>

namespace CthunAgent {

class RequestProcessor {
  public:
    RequestProcessor() = delete;

    explicit RequestProcessor(std::shared_ptr<CthunConnector> connector_ptr);

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
    void processRequest(std::shared_ptr<Module> module_ptr,
                        const ActionRequest& request);

  private:
    /// Manages the lifecycle of non-blocking action jobs
    ThreadContainer thread_container_;

    /// Cthun Connector pointer
    std::shared_ptr<CthunConnector> connector_ptr_;

    /// Where the directories for non-blocking actions results will
    /// be created
    const std::string spool_dir_;

    void processBlockingRequest(std::shared_ptr<Module> module_ptr,
                                const ActionRequest& request);

    void processNonBlockingRequest(std::shared_ptr<Module> module_ptr,
                                   const ActionRequest& request);

};

}  // namespace CthunAgent

#endif  // SRC_AGENT_REQUEST_PROCESSOR_HPP_
