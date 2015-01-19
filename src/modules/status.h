#ifndef SRC_MODULES_STATUS_H_
#define SRC_MODULES_STATUS_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Status : public CthunAgent::Module {
  public:
    Status();
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

#endif  // SRC_MODULES_STATUS_H_
