#ifndef SRC_MODULES_PING_H_
#define SRC_MODULES_PING_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Ping : public CthunAgent::Module {
  public:
    Ping();
    DataContainer call_action(std::string action_name,
                              const Message& request);

    void call_delayed_action(std::string action_name,
                             const Message& request,
                             std::string job_id) {}

    /// Ping action calculates the time it took for a message to travel from
    /// the controller to the agent. It will then add that duration and the
    /// current server time in milliseconds to the response.
    DataContainer ping_action(const Message& request);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_PING_H_
