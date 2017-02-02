#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/configuration.hpp>

#include <cpp-pcp-client/protocol/parsed_chunks.hpp>

#include <memory>
#include <string>

namespace PXPAgent {

class PXPConnector;

class Agent {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    struct PCPConfigurationError : public Error {
        explicit PCPConfigurationError(std::string const& msg) : Error(msg) {}
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
    //  - connecting to the PCP broker;
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

    // Ping interval in seconds
    uint32_t ping_interval_s_;

    // Callback for PCPClient::Connector handling incoming PXP
    // blocking requests; it will execute the requested action and,
    // once finished, reply to the sender with an PXP blocking
    // response containing the action outcome.
    void blockingRequestCallback(const PCPClient::v1::ParsedChunks&);

    // Callback for PCPClient::Connector handling incoming PXP
    // non-blocking requests; it will start a job for the requested
    // action and reply with a provisional response containing the job
    // id. The reults will be stored in files in spool-dir.
    // In case the request has the notify_outcome field flagged, it
    // will send a PXP non-blocking response containing the action
    // outcome when finished.
    void nonBlockingRequestCallback(const PCPClient::v1::ParsedChunks&);
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ENDPOINT_H_
