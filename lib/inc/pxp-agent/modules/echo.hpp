#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include <pxp-agent/module.hpp>

namespace PXPAgent {
namespace Modules {

class Echo : public PXPAgent::Module {
  public:
    Echo();

  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_ECHO_H_
