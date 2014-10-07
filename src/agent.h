#ifndef SRC_AGENT_H_
#define SRC_AGENT_H_

#include "module.h"

#include <cthun-client/src/connection_manager.h>

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <memory>

namespace CthunAgent {

//
// HeartbeatTask
//

class HeartbeatTask {
  public:
    explicit HeartbeatTask(Cthun::Client::Connection::Ptr connection_ptr);
    ~HeartbeatTask();
    void start();
    void stop();

  private:
    bool must_stop_;
    std::thread heartbeat_thread_;
    std::string binary_payload_ { "cthun ping payload" };
    Cthun::Client::Connection::Ptr connection_ptr_ { nullptr };

    void heartbeatThread();
};

//
// Agent
//

class Agent {
  public:
    Agent();
    ~Agent();

    // for development purposes
    void run(std::string module, std::string action);

    // daemon entry point
    void connect_and_run(std::string url,
                         std::string ca_crt_path,
                         std::string client_crt_path,
                         std::string client_key_path);

  private:
    std::unique_ptr<HeartbeatTask> heartbeat_task_;

    void list_modules();
    void send_login(Cthun::Client::Client_Type* client_ptr);
    void handle_message(Cthun::Client::Client_Type* client_ptr,
                        std::string message);

    std::map<std::string, std::shared_ptr<Module>> modules_;
    Cthun::Client::Connection::Ptr connection_ptr_ { nullptr };
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_H_
