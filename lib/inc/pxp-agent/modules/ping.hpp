#ifndef SRC_MODULES_PING_H_
#define SRC_MODULES_PING_H_

#include <pxp-agent/module.hpp>

namespace PXPAgent {
namespace Modules {

class Ping : public PXPAgent::Module {
  public:
    Ping();

    // NOTE(ale): public for testing
    /// Ping calculates the time it took for a message to travel from
    /// the controller to the agent. It will then add that duration
    /// and the current broker time in milliseconds to the response.
    lth_jc::JsonContainer ping(const ActionRequest& request);

  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_PING_H_
