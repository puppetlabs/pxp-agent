#ifndef SRC_MODULES_INVENTORY_H_
#define SRC_MODULES_INVENTORY_H_

#include "../module.h"

namespace Cthun {
namespace Agent {
namespace Modules {

class Inventory : public Cthun::Agent::Module {
  public:
    Inventory();
    void call_action(std::string action_name, const Json::Value& input,
                     Json::Value& output);
};

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_MODULES_INVENTORY_H_
