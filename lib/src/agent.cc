#include <pxp-agent/agent.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#include <cpp-pcp-client/protocol/schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>

namespace PXPAgent {

Agent::Agent(const Configuration::Agent& agent_configuration)
        try
            : connector_ptr_ { new PXPConnector(agent_configuration) },
              request_processor_ { connector_ptr_, agent_configuration } {
} catch (const PCPClient::connection_config_error& e) {
    throw Agent::WebSocketConfigurationError { e.what() };
}

void Agent::start() {
    // TODO(ale): add associate response callback

    connector_ptr_->registerMessageCallback(
        PXPSchemas::BlockingRequestSchema(),
        [this](const PCPClient::ParsedChunks& parsed_chunks) {
            blockingRequestCallback(parsed_chunks);
        });

    connector_ptr_->registerMessageCallback(
        PXPSchemas::NonBlockingRequestSchema(),
        [this](const PCPClient::ParsedChunks& parsed_chunks) {
            nonBlockingRequestCallback(parsed_chunks);
        });

    connector_ptr_->registerMessageCallback(
        PCPClient::Protocol::TTLExpiredSchema(),
        [this](const PCPClient::ParsedChunks& parsed_chunks) {
            ttlExpiredCallback(parsed_chunks);
        });

    try {
        connector_ptr_->connect();
    } catch (const PCPClient::connection_config_error& e) {
        // Failed to configure WebSocket on our end
        throw Agent::WebSocketConfigurationError { e.what() };
    } catch (const PCPClient::connection_fatal_error& e) {
        // Unexpected, as we didn't set max num retries
        throw Agent::FatalError { e.what() };
    }

    // The agent is now connected and the request handlers are set;
    // we can now call the monitoring method that will block this
    // thread of execution.
    // Note that, in case the underlying connection drops, the
    // connector will keep trying to re-establish it indefinitely
    // (the max_connect_attempts is 0 by default).
    try {
        connector_ptr_->monitorConnection();
    } catch (const PCPClient::connection_fatal_error& e) {
        // Unexpected, as we didn't set max num retries
        throw Agent::FatalError { e.what() };
    }
}

void Agent::blockingRequestCallback(const PCPClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::Blocking, parsed_chunks);
}

void Agent::nonBlockingRequestCallback(const PCPClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::NonBlocking, parsed_chunks);
}

void Agent::ttlExpiredCallback(const PCPClient::ParsedChunks& parsed_chunks) {
    LOG_WARNING("Received TTL expired message - expired message ID: %1%",
                parsed_chunks.data.get<std::string>("id"));
}

}  // namespace PXPAgent
