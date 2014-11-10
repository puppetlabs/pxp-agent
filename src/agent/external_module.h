#ifndef SRC_AGENT_EXTERNAL_MODULE_H_
#define SRC_AGENT_EXTERNAL_MODULE_H_

#include "src/agent/module.h"

#include <map>
#include <string>

namespace Cthun {
namespace Agent {

class ExternalModule : public Module {
  public:
    explicit ExternalModule(std::string path);
    void call_action(std::string action_name,
                     const Json::Value& request,
                     const Json::Value& input,
                     Json::Value& output);
    void call_delayed_action(std::string action_name,
                             const Json::Value& request,
                             const Json::Value& input,
                             Json::Value& output,
                             std::string action_id);
  private:
    std::string path_;
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_EXTERNAL_MODULE_H_
