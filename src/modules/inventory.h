#ifndef SRC_MODULES_INVENTORY_H_
#define SRC_MODULES_INVENTORY_H_

#include "../module.h"

namespace CthunAgent {
namespace Modules {

class Inventory : public CthunAgent::Module {
  public:
    Inventory();
    void call_action(std::string name,
                     const Json::Value& input,
                     Json::Value& output);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_INVENTORY_H_
