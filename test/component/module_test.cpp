#include "test/test.h"

#include "src/agent/agent_endpoint.h"
#include "src/agent/module.h"

namespace Cthun {
namespace Agent {

TEST_CASE("Module::call_action", "[modules]") {
    Agent::AgentEndpoint agent { "./../../bin/cthun_agent" };

    SECTION("it should correctly call an existent action") {

    }
}


}  // namespace Agent
}  // namespace Cthun
