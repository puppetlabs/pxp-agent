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
    std::map<std::string,Action> actions;
    virtual void call_action(std::string action, const Json::Value& input, Json::Value& output) = 0;
    void validate_and_call_action(std::string action, const Json::Value& input, Json::Value& output);
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
