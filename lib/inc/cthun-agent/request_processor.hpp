#ifndef SRC_AGENT_REQUEST_PROCESSOR_HPP_
#define SRC_AGENT_REQUEST_PROCESSOR_HPP_

#include <cthun-agent/module.hpp>
#include <cthun-agent/thread_container.hpp>
#include <cthun-agent/action_request.hpp>

#include <cthun-client/connector/connector.hpp>

#include <memory>
#include <string>

namespace CthunAgent {

static const int DEFAULT_MSG_TIMEOUT_SEC { 10 };

class RequestProcessor {
  public:
    RequestProcessor() = delete;

    explicit RequestProcessor(std::shared_ptr<CthunClient::Connector> connector_ptr);

    /// Execute the specified action. Once it's done, send back to the
    /// requester a blocking response containing the action results.
    /// Propagates possible request errors raised by the action logic.
    /// In case it fails to send the response, no further attempt will
    /// be made.
    void processBlockingRequest(std::shared_ptr<Module> module_ptr,
                                const ActionRequest& request);

    /// Start a task for the specified action in a separate execution
    /// thread. Once the task has started, send a provisional response
    /// to the requester. In case the request has the notify_outcome
    /// field flagged, the task will send a non-blocking response
    /// containing the action outcome, after the action is done. The
    /// task will also write the action outcome and request metadata
    /// to disk.
    void processNonBlockingRequest(std::shared_ptr<Module> module_ptr,
                                   const ActionRequest& request);

  private:
    /// Cthun Connector pointer
    std::shared_ptr<CthunClient::Connector> connector_ptr_;

    /// Where the directories for non-blocking actions results will
    /// be created
    const std::string spool_dir_;

    /// Manages the lifecycle of non-blocking action jobs
    ThreadContainer thread_container_;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_REQUEST_PROCESSOR_HPP_
