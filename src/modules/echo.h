#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include "../module.h"

namespace CthunAgent {
namespace Modules {

class Echo : public CthunAgent::Module {
public:
    Echo();
    void call_action(std::string name,  const Json::Value& input, Json::Value& output);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_ECHO_H_
