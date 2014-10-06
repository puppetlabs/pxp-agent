#ifndef SRC_MODULES_PING_H_
#define SRC_MODULES_PING_H_

#include "../module.h"

namespace CthunAgent {
namespace Modules {

class Ping : public CthunAgent::Module {
  public:
    Ping();
    void call_action(std::string name, const Json::Value& input, Json::Value& output);
    /// Ping action calculates the time it took for a message to travel from
    /// the controller to the agent. It will then add that duration and the
    /// current server time in milliseconds to the response.
    void ping_action(const Json::Value& input, Json::Value& output);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_PING_H_
