#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include "src/module.h"
#include "src/data_container.h"
#include "src/websocket/endpoint.h"

#include <map>
#include <memory>
#include <string>

namespace CthunAgent {

class Agent {
  public:
    explicit Agent(std::string bin_path);
    ~Agent();

    // Daemon entry point.
    void startAgent(std::string url,
                    std::string ca_crt_path,
                    std::string client_crt_path,
                    std::string client_key_path);

  private:
    std::map<std::string, std::shared_ptr<Module>> modules_;
    std::unique_ptr<WebSocket::Endpoint> ws_endpoint_ptr_;

    // Log the loaded modules.
    void listModules();

    // Set the event callbacks for the connection.
    void setConnectionCallbacks();

    // Send a login message.
    void sendLogin();

    // Throw a message_validation_error in case of invalid message.
    Message parseAndValidateMessage(std::string message);

    // Send a response message with specified request ID and output
    // to the receiver endpoint.
    void sendResponse(std::string receiver_endpoint,
                      std::string request_id,
                      DataContainer output);

    // Parse and validate the passed message; reply to the sender
    // with the requested output.
    void processMessageAndSendResponse(std::string message);

    // Periodically check the connection state; reconnect the agent
    // in case the connection is not open
    void monitorConnectionState();
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
