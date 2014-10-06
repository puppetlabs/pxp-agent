#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <map>
#include <string>

#include <json/json.h>

#include "action.h"

namespace CthunAgent {

class Module {
  public:
    std::string name;
    std::map<std::string, Action> actions;

    virtual void call_action(std::string action,
                             const Json::Value& input,
                             Json::Value& output) = 0;

    /// Validate the json schemas of input and output.
    /// Execute the requested action for the particular module.
    /// Sets an error response in the referred output json object
    /// in case of unknown action or invalid schemas.
    void validate_and_call_action(std::string action,
                                  const Json::Value& input,
                                  Json::Value& output);
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
