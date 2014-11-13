#ifndef SRC_AGENT_AGENT_ENDPOINT_H_
#define SRC_AGENT_AGENT_ENDPOINT_H_

#include "src/agent/module.h"
#include "src/websocket/endpoint.h"

#include <json/json.h>

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Cthun {
namespace Agent {

class AgentEndpoint {
  public:
    AgentEndpoint();
    ~AgentEndpoint();

    // daemon entry point
    void startAgent(std::string url,
                    std::string ca_crt_path,
                    std::string client_crt_path,
                    std::string client_key_path);

  private:
    std::map<std::string, std::shared_ptr<Module>> modules_;
    std::unique_ptr<Cthun::WebSocket::Endpoint> ws_endpoint_ptr_;

    // Thread queue...sigh
    std::vector<std::thread> thread_queue_;

    // log the loaded modules
    void listModules();

    // set WebSocket event callbacks
    void setConnectionCallbacks();

    // WebSocket onOpen event callback
    void sendLogin();

    // throw a validation_error in case of invalid message
    Json::Value parseAndValidateMessage(std::string message);

    // send a response message with specified request ID
    void sendResponse(std::string receiver_endpoint,
                      std::string request_id,
                      Json::Value output);

    // WebSocket onMessage event callback
    void processMessageAndSendResponse(std::string message);

    // TODO: keep attempting to reconnect
    // periodically check the connection state; make a single attempt
    // to reconnect the agent in case the connection is not open
    void monitorConnectionState();

    // task to validate and execute the specified action
    void delayedActionThread(std::shared_ptr<Module> module,
                             std::string action_name,
                             Json::Value doc,
                             Json::Value output,
                             std::string uuid);
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_AGENT_ENDPOINT_H_
