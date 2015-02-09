#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Echo : public CthunAgent::Module {
  public:
    Echo();
    DataContainer callAction(const std::string& action_name,
                             const DataContainer& request);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_ECHO_H_
