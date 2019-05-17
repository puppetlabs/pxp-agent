#ifndef SRC_MODULES_COMMAND_H_
#define SRC_MODULES_COMMAND_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/action_response.hpp>
#include <leatherman/curl/client.hpp>
#include <leatherman/execution/execution.hpp>

namespace PXPAgent {
namespace Modules {

class Command : public PXPAgent::Module {
  public:
    Command();
    void processOutputAndUpdateMetadata(ActionResponse& response) override;
    virtual leatherman::execution::result run(const ActionRequest& request);

  private:
    ActionResponse callAction(const ActionRequest& request) override;
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_COMMAND_H_
