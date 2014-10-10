#ifndef SRC_AGENT_EXTERNAL_MODULE_H_
#define SRC_AGENT_EXTERNAL_MODULE_H_

#include "module.h"

#include <map>
#include <string>

namespace Cthun {
namespace Agent {

class ExternalModule : public Module {
  public:
    explicit ExternalModule(std::string path);
    void call_action(std::string action_name, const Json::Value& input,
                     Json::Value& output);
  private:
    std::string path_;
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_EXTERNAL_MODULE_H_
