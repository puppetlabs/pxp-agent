#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/action_response.hpp>

namespace PXPAgent {
namespace Modules {

class Echo : public PXPAgent::Module {
  public:
    Echo();

  private:
    ActionResponse callAction(const ActionRequest& request);
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_ECHO_H_
