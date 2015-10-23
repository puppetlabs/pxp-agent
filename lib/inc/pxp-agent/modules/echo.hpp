#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/export.h>

namespace PXPAgent {
namespace Modules {

class LIBPXP_AGENT_EXPORT Echo : public PXPAgent::Module {
  public:
    Echo();

  private:
    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_ECHO_H_
