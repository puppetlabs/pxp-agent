#include <pxp-agent/agent.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.agent"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

Agent::Agent(const Configuration::Agent& agent_configuration)
    : connector_ptr_ { new PXPConnector(agent_configuration) },
    request_processor_ { connector_ptr_, agent_configuration } {
}

void Agent::start() {
    // Callback for PXPConnector handling incoming PXP
    // blocking requests; it will execute the requested action and,
    // once finished, reply to the sender with an PXP blocking
    // response containing the action outcome.
    connector_ptr_->registerMessageCallback(
        PXPSchemas::BlockingRequestSchema(),
        [this](std::string id,
               std::string sender,
               leatherman::json_container::JsonContainer data,
               std::vector<leatherman::json_container::JsonContainer> debug) {
            request_processor_.processRequest(RequestType::Blocking,
                                              std::move(id),
                                              std::move(sender),
                                              std::move(data),
                                              std::move(debug));
        });

    // Callback for PXPConnector handling incoming PXP
    // non-blocking requests; it will start a job for the requested
    // action and reply with a provisional response containing the job
    // id. The reults will be stored in files in spool-dir.
    // In case the request has the notify_outcome field flagged, it
    // will send a PXP non-blocking response containing the action
    // outcome when finished.
    connector_ptr_->registerMessageCallback(
        PXPSchemas::NonBlockingRequestSchema(),
        [this](std::string id,
               std::string sender,
               leatherman::json_container::JsonContainer data,
               std::vector<leatherman::json_container::JsonContainer> debug) {
            request_processor_.processRequest(RequestType::NonBlocking,
                                              std::move(id),
                                              std::move(sender),
                                              std::move(data),
                                              std::move(debug));
        });

    connector_ptr_->connect();
}

}  // namespace PXPAgent
