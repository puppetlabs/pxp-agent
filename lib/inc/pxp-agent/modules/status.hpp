#ifndef SRC_MODULES_STATUS_H_
#define SRC_MODULES_STATUS_H_

#include <pxp-agent/module.hpp>

namespace PXPAgent {
namespace Modules {

class Status : public PXPAgent::Module {
  public:
    Status();
  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_STATUS_H_
