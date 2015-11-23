#include "certs.hpp"

#include "root_path.hpp"

#include <pxp-agent/agent.hpp>
#include <pxp-agent/configuration.hpp>

#include <catch.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

namespace PXPAgent {

const std::string TEST_BROKER_WS_URI { "wss://127.0.0.1:8090/pxp/" };
static const std::string MODULES { std::string { PXP_AGENT_ROOT_PATH }
                                   + "/lib/tests/resources/modules" };
const std::string SPOOL { std::string { PXP_AGENT_ROOT_PATH }
                          + "/lib/tests/resources/tmp/" };

TEST_CASE("Agent::Agent", "[agent]") {
    Configuration::Agent agent_configuration { MODULES,
                                               TEST_BROKER_WS_URI,
                                               getCaPath(),
                                               getCertPath(),
                                               getKeyPath(),
                                               SPOOL,
                                               "",  // modules config dir
                                               "test_agent",
                                               5000 };

    SECTION("does not throw if it fails to find the external modules directory") {
        agent_configuration.modules_dir = MODULES + "/fake_dir";

        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    SECTION("should throw an Agent::Error if client cert path is invalid") {
        agent_configuration.crt = "spam";

        REQUIRE_THROWS_AS(Agent { agent_configuration }, Agent::Error);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    boost::filesystem::remove_all(SPOOL);
}

}  // namespace PXPAgent
