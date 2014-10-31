#ifndef SRC_AGENT_AGENT_ENDPOINT_H_
#define SRC_AGENT_AGENT_ENDPOINT_H_

#include "src/agent/module.h"
#include "src/websocket/connection_manager.h"

#include <json/json.h>

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>

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
    std::string binary_payload_ { "cthun ping payload" };
    std::map<std::string, std::shared_ptr<Module>> modules_;
    Cthun::WebSocket::Connection::Ptr connection_ptr_ { nullptr };

    // Thread queue...sigh
    std::vector<std::thread> thread_queue_;

    // log  the loaded modules
    void listModules();

    // on open event callback
    void sendLogin(Cthun::WebSocket::Client_Type* client_ptr);

    // throw a validation_error in case of invalid message
    Json::Value parseAndValidateMessage(std::string message);

    // on message event callback
    void handleMessage(Cthun::WebSocket::Client_Type* client_ptr,
                       std::string message);

    // set WebSocket event callbacks
    void setConnectionCallbacks();

    // periodically check the connection state; make a single attempt
    // to reconnect the agent in case the connection is not open
    void monitorConnectionState();

    void sendResponseMessage(std::string sender, Json::Value output, Cthun::WebSocket::Client_Type* client_ptr);

    void delayedActionThread(std::shared_ptr<Module> module,
                          std::string action_name,
                          Json::Value doc,
                          Json::Value output,
                          std::string uuid);
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_AGENT_ENDPOINT_H_
