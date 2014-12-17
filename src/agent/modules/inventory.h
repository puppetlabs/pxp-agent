#ifndef SRC_AGENT_MODULES_INVENTORY_H_
#define SRC_AGENT_MODULES_INVENTORY_H_

#include "src/agent/module.h"

namespace Cthun {
namespace Agent {
namespace Modules {

class Inventory : public Cthun::Agent::Module {
  public:
    Inventory();
    DataContainer call_action(std::string action_name,
                     const Message& request,
                     const DataContainer& input);

    void call_delayed_action(std::string action_name,
                             const Message& request,
                             const DataContainer& input,
                             std::string job_id) {}
};

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_MODULES_INVENTORY_H_
