#include <pxp-agent/agent.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#include <cpp-pcp-client/protocol/schemas.hpp>

#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>
#include <stdint.h>
#include <random>

namespace PXPAgent {

namespace pcp_util = PCPClient::Util;

// Pause between PCP connection attempts after Association errors
static const uint32_t ASSOCIATE_SESSION_TIMEOUT_PAUSE_S { 5 };

Agent::Agent(const Configuration::Agent& agent_configuration)
        try
            : connector_ptr_ { new PXPConnector(agent_configuration) },
              request_processor_ { connector_ptr_, agent_configuration } {
} catch (const PCPClient::connection_config_error& e) {
    throw Agent::WebSocketConfigurationError { e.what() };
}

void Agent::start() {
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

    int num_seconds {};
    std::random_device rd {};
    std::default_random_engine engine { rd() };
    std::uniform_int_distribution<int> dist { ASSOCIATE_SESSION_TIMEOUT_PAUSE_S,
                                              2 * ASSOCIATE_SESSION_TIMEOUT_PAUSE_S };

    try {
        do {
            try {
                connector_ptr_->connect();
                break;
            } catch (const PCPClient::connection_association_error& e) {
                num_seconds = dist(engine);
                LOG_WARNING("Error during the PCP Session Association ({1}); "
                            "will retry to connect in {2} s",
                            e.what(), num_seconds);
                pcp_util::this_thread::sleep_for(
                    pcp_util::chrono::seconds(num_seconds));
            }
        } while (true);

        // The agent is now connected and the request handlers are
        // set; we can now call the monitoring method that will block
        // this thread of execution.
        // Note that, in case the underlying connection drops, the
        // connector will keep trying to re-establish it indefinitely
        // (the max_connect_attempts is 0 by default).
        LOG_DEBUG("PCP connection established; about to start monitoring it");
        connector_ptr_->monitorConnection();
    } catch (const PCPClient::connection_config_error& e) {
        // WebSocket configuration failure
        throw Agent::WebSocketConfigurationError { e.what() };
    } catch (const PCPClient::connection_association_response_failure& e) {
        // Associate Session failure; this should be due to a
        // configuration mismatch with the broker (incompatible
        // PCP versions?)
        throw Agent::PCPConfigurationError { e.what() };
    } catch (const PCPClient::connection_fatal_error& e) {
        // Unexpected, as we're not limiting the num retries, for
        // both connect() and monitorConnection() calls
        throw Agent::FatalError { e.what() };
    }
}

void Agent::blockingRequestCallback(const PCPClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::Blocking, parsed_chunks);
}

void Agent::nonBlockingRequestCallback(const PCPClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::NonBlocking, parsed_chunks);
}

}  // namespace PXPAgent
