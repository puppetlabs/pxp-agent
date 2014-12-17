#ifndef SRC_AGENT_MODULES_PING_H_
#define SRC_AGENT_MODULES_PING_H_

#include "src/agent/module.h"

namespace Cthun {
namespace Agent {
namespace Modules {

class Ping : public Cthun::Agent::Module {
  public:
    Ping();
    DataContainer call_action(std::string action_name,
                     const Message& request,
                     const DataContainer& input);

    void call_delayed_action(std::string action_name,
                             const Message& request,
                             const DataContainer& input,
                             std::string job_id) {}

    /// Ping action calculates the time it took for a message to travel from
    /// the controller to the agent. It will then add that duration and the
    /// current server time in milliseconds to the response.
    DataContainer ping_action(const Message& request,
                     const DataContainer& input);
};

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_MODULES_PING_H_
