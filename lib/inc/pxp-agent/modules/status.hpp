#ifndef SRC_MODULES_STATUS_H_
#define SRC_MODULES_STATUS_H_

#include <pxp-agent/module.hpp>

namespace PXPAgent {
namespace Modules {

class Status : public PXPAgent::Module {
  public:
    static const std::string UNKNOWN;
    static const std::string SUCCESS;
    static const std::string FAILURE;
    static const std::string RUNNING;

    Status();
  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_STATUS_H_
