#ifndef SRC_AGENT_H_
#define SRC_AGENT_H_

#include <map>
#include <memory>
#include "module.h"
#include "client.h"

namespace CthunAgent {

class Agent {
  public:
    Agent();
    // daemon entry point
    void connect_and_run();
    void run(std::string module, std::string action);
    void list_modules();
    void send_login();
  private:
    std::map<std::string,std::shared_ptr<Module>> modules_;
    Client client_;
    Cthun::Client::Connection_ID connection_;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_H_
