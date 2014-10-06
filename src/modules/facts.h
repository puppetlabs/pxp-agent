#ifndef SRC_MODULES_FACTS_H_
#define SRC_MODULES_FACTS_H_

#include "../module.h"

namespace CthunAgent {
namespace Modules {

class Facts : public CthunAgent::Module {
  public:
    Facts();
    void call_action(std::string name,
                     const Json::Value& input,
                     Json::Value& output);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_FACTS_H_
