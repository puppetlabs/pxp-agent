#ifndef SRC_MODULES_PING_H_
#define SRC_MODULES_PING_H_

#include <cthun-agent/module.hpp>

namespace CthunAgent {
namespace Modules {

class Ping : public CthunAgent::Module {
  public:
    Ping();

    // NOTE(ale): public for testing
    /// Ping calculates the time it took for a message to travel from
    /// the controller to the agent. It will then add that duration
    /// and the current server time in milliseconds to the response.
    CthunClient::DataContainer ping(const ActionRequest& request);

  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_PING_H_
