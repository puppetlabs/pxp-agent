#ifndef SRC_AGENT_MODULE_H_
#define SRC_AGENT_MODULE_H_

#include "src/agent/action.h"

#include <json/json.h>

#include <map>
#include <string>

namespace Cthun {
namespace Agent {

class Module {
  public:
    std::string module_name;
    std::map<std::string, Action> actions;

    virtual void call_action(std::string action_name, const Json::Value& input,
                             Json::Value& output) = 0;

    /// Validate the json schemas of input and output.
    /// Execute the requested action for the particular module.
    /// Sets an error response in the referred output json object
    /// in case of unknown action or invalid schemas.
    void validate_and_call_action(std::string action_name,
                                  const Json::Value& input,
                                  Json::Value& output);
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_MODULE_H_
