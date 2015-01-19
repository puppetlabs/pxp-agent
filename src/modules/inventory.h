#ifndef SRC_MODULES_INVENTORY_H_
#define SRC_MODULES_INVENTORY_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Inventory : public CthunAgent::Module {
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
}  // namespace CthunAgent

#endif  // SRC_MODULES_INVENTORY_H_
