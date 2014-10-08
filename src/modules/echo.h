#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include "../module.h"

namespace Cthun {
namespace Agent {
namespace Modules {

class Echo : public Cthun::Agent::Module {
  public:
    Echo();
    void call_action(std::string action_name, const Json::Value& input,
                     Json::Value& output);
};

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_MODULES_ECHO_H_
