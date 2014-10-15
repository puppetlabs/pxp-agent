#ifndef SRC_AGENT_MODULES_STATUS_H_
#define SRC_AGENT_MODULES_STATUS_H_

#include "src/agent/module.h"

namespace Cthun {
namespace Agent {
namespace Modules {

class Status : public Cthun::Agent::Module {
  public:
    Status();
    void call_action(std::string action_name, const Json::Value& input,
                     Json::Value& output);
    void call_delayed_action(std::string action_name,
                             const Json::Value& input,
                             Json::Value& output,
                             std::string job_id){};
};

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_MODULES_INVENTORY_H_
