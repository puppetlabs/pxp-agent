#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/configuration.hpp>

#include <memory>
#include <string>

namespace PXPAgent {

class Agent {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    struct ConfigurationError : public Error {
        explicit ConfigurationError(std::string const& msg) : Error(msg) {}
    };

    struct WebSocketConfigurationError : public Error {
        explicit WebSocketConfigurationError(std::string const& msg) : Error(msg) {}
    };

    struct FatalError : public Error {
        explicit FatalError(std::string const& msg) : Error(msg) {}
    };

    Agent() = delete;

    // Configure the pxp-agent run by:
    //  - instantiating PXPConnector;
    //  - instantiating a RequestProcessor.
    //
    // Throw an Agent::WebSocketConfigurationError in case it fails to
    // determine the agent identity by inspecting the SSL certificate.
    Agent(const Configuration::Agent& agent_configuration);

    // Start the agent and loop indefinitely, by:
    //  - registering message callbacks;
    //  - connecting to the broker;
    //  - monitoring the state of the connection;
    //  - re-establishing the connection when requested.
    //
    // Throw an Agent::WebSocketConfigurationError in case it fails to
    // set up the Websocket connection on this end (ex. TLS layer
    // error) or an Agent::FatelError in case of unexpected failures;
    // errors such as message sending failures are only logged.
    void start();

  private:
    // PXP connector
    std::shared_ptr<PXPConnector> connector_ptr_;

    // Request Processor
    RequestProcessor request_processor_;
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ENDPOINT_H_
