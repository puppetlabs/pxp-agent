#ifndef SRC_AGENT_AGENT_ENDPOINT_H_
#define SRC_AGENT_AGENT_ENDPOINT_H_

#include "src/agent/module.h"
#include "src/websocket/endpoint.h"
#include "src/data_container.h"

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Cthun {
namespace Agent {

class AgentEndpoint {
  public:
    explicit AgentEndpoint(std::string bin_path);
    ~AgentEndpoint();

    // Daemon entry point.
    void startAgent(std::string url,
                    std::string ca_crt_path,
                    std::string client_crt_path,
                    std::string client_key_path);

  private:
    std::map<std::string, std::shared_ptr<Module>> modules_;
    std::unique_ptr<Cthun::WebSocket::Endpoint> ws_endpoint_ptr_;

    // Thread queue...sigh
    std::vector<std::thread> thread_queue_;

    // Log the loaded modules.
    void listModules();

    // Set the event callbacks for the connection.
    void setConnectionCallbacks();

    // Send a login message.
    void sendLogin();

    // Throw a validation_error in case of invalid message.
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

    // Task to validate and execute the specified action.
    void delayedActionThread(std::shared_ptr<Module> module,
                             std::string action_name,
                             Message msg,
                             std::string uuid);
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_AGENT_ENDPOINT_H_
