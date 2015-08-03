#include <cthun-agent/agent.hpp>
#include <cthun-agent/rpc_schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>

namespace CthunAgent {

Agent::Agent(const Configuration::Agent& agent_configuration)
        try
            : connector_ptr_ { new CthunConnector(agent_configuration) },
              request_processor_ { connector_ptr_, agent_configuration } {
} catch (CthunClient::connection_config_error& e) {
    throw Agent::Error { std::string { "failed to configure: " } + e.what() };
}

void Agent::start() {
    // TODO(ale): add associate response callback

    connector_ptr_->registerMessageCallback(
        RPCSchemas::BlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            blockingRequestCallback(parsed_chunks);
        });

    connector_ptr_->registerMessageCallback(
        RPCSchemas::NonBlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            nonBlockingRequestCallback(parsed_chunks);
        });

    try {
        connector_ptr_->connect();
    } catch (CthunClient::connection_config_error& e) {
        LOG_ERROR("Failed to configure the underlying communications layer: %1%",
                  e.what());
        throw Agent::Error { "failed to configure the underlying communications"
                             "layer" };
    } catch (CthunClient::connection_fatal_error& e) {
        LOG_ERROR("Failed to connect: %1%", e.what());
        throw Agent::Error { "failed to connect" };
    }

    // The agent is now connected and the request handlers are set;
    // we can now call the monitoring method that will block this
    // thread of execution.
    // Note that, in case the underlying connection drops, the
    // connector will keep trying to re-establish it indefinitely
    // (the max_connect_attempts is 0 by default).
    try {
        connector_ptr_->monitorConnection();
    } catch (CthunClient::connection_fatal_error) {
        throw Agent::Error { "failed to reconnect" };
    }
}

void Agent::blockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::Blocking, parsed_chunks);
}

void Agent::nonBlockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::NonBlocking, parsed_chunks);
}

}  // namespace CthunAgent
