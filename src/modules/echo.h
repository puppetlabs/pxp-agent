#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Echo : public CthunAgent::Module {
  public:
    Echo();
    DataContainer call_action(std::string action_name,
                              const Message& request,
                              const DataContainer& input);

    void call_delayed_action(std::string action_name,
                             const Message& request,
                             const DataContainer& input,
                             std::string job_idd) {}
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_ECHO_H_
