#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include "src/module.h"
#include "src/message.h"

namespace CthunAgent {
namespace Modules {

class Echo : public CthunAgent::Module {
  public:
    Echo();
    DataContainer callAction(const std::string& action_name,
                             const ParsedContent& request);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_ECHO_H_
