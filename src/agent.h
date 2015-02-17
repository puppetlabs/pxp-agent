#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include "src/module.h"
#include "src/message.h"
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

    // Callback for the transport layer triggered when  connection .
    // Send a login message.
    void sendLogin();

    // Add the common envelope entries to the specified container.
    void addCommonEnvelopeEntries(DataContainer& envelope_entries);

    // Send a response message with specified request ID and output
    // to the receiver endpoint.
    void sendResponse(std::string receiver_endpoint,
                      std::string request_id,
                      DataContainer output,
                      std::vector<MessageChunk> debug_chunks);

    // Callback for the transport layer to handle incoming messages.
    // Parse and validate the passed message; reply to the sender
    // with the requested output.
    void processMessageAndSendResponse(std::string message_txt);

    // Periodically check the connection state; reconnect the agent
    // in case the connection is not open
    void monitorConnectionState();
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
