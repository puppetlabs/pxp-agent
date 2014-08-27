#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include <map>
#include <string>
#include "module.h"

namespace CthunAgent {

class ExternalModule : public Module {
  public:
    explicit ExternalModule(std::string path);
    std::string name;
    std::map<std::string,Action> actions;

    void call_action(std::string action, const Json::Value& input, Json::Value& output);
  private:
    std::string path_;

};

}  // namespace CthunAgent

#endif  // SRC_EXTERNAL_MODULE_H_
