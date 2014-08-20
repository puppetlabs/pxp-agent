#include "agent.h"
#include "modules/echo.h"

namespace puppetlabs {
namespace cthun {

Agent::Agent() {
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);
}

void Agent::run() {
}


}  // namespace cthun
}  // namespace puppetlabs
