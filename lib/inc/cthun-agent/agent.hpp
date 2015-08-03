#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include <cthun-agent/request_processor.hpp>
#include <cthun-agent/action_request.hpp>
#include <cthun-agent/cthun_connector.hpp>
#include <cthun-agent/configuration.hpp>

#include <cthun-client/protocol/chunks.hpp>      // ParsedChunk

#include <memory>
#include <string>

namespace CthunAgent {

class Agent {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    Agent() = delete;

    // Configure the cthun-agent run by:
    //  - instantiating CthunConnector;
    //  - instantiating a RequestProcessor.
    //
    // Throw an Agent::Error in case it fails to determine the agent
    // identity by inspecting the certificate.
    Agent(const Configuration::Agent& agent_configuration);

    // Start the agent and loop indefinitely, by:
    //  - registering message callbacks;
    //  - connecting to the Cthun server;
    //  - monitoring the state of the connection;
    //  - re-establishing the connection when requested.
    //
    // Throw an Agent::Error in case of unexpected failures; errors
    // such as message sending failures are only logged.
    void start();

  private:
    // Cthun connector
    std::shared_ptr<CthunConnector> connector_ptr_;

    // Request Processor
    RequestProcessor request_processor_;

    // Callback for CthunClient::Connector handling incoming RPC
    // blocking requests; it will execute the requested action and,
    // once finished, reply to the sender with an RPC blocking
    // response containing the action outcome.
    void blockingRequestCallback(const CthunClient::ParsedChunks& parsed_chunks);

    // Callback for CthunClient::Connector handling incoming RPC
    // non-blocking requests; it will start a job for the requested
    // action and reply with a provisional response containing the job
    // id. The reults will be stored in files in spool-dir.
    // In case the request has the notify_outcome field flagged, it
    // will send an RPC non-blocking response containing the action
    // outcome when finished.
    void nonBlockingRequestCallback(const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
