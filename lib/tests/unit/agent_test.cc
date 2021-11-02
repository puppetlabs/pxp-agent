#include "../common/certs.hpp"

#include "root_path.hpp"

#include <pxp-agent/agent.hpp>
#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.test"

#include <leatherman/logging/logging.hpp>

#include <catch.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

using namespace PXPAgent;

const std::vector<std::string> TEST_BROKER_WS_URIS { "wss://127.0.0.1:8090/pxp/" };
static const std::string MODULES { std::string { PXP_AGENT_ROOT_PATH }
                                   + "/lib/tests/resources/modules" };
const std::string SPOOL { std::string { PXP_AGENT_ROOT_PATH }
                          + "/lib/tests/resources/tmp/" };

TEST_CASE("Agent::Agent", "[agent]") {
    Configuration::Agent agent_configuration { MODULES,
                                               TEST_BROKER_WS_URIS,
                                               std::vector<std::string> {},  // master uris
                                               "1",   // PCPv1
                                               getCaPath(),
                                               getCertPath(),
                                               getKeyPath(),
                                               getCrlPath(),
                                               SPOOL,
                                               "0d",  // don't purge spool!
                                               "",    // modules config dir
                                               "",    // task cache dir
                                               "0d",  // don't purge task cache!
                                               "test_agent",
                                               "",    // don't set broker proxy
                                               "",    // don't set master proxy
                                               5000, 10, 5, 5, 2, 15, 30, 120, 1024,
                                               leatherman::logging::log_level::none };

    SECTION("does not throw if it fails to find the external modules directory") {
        agent_configuration.modules_dir = MODULES + "/fake_dir";

        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    SECTION("should throw an Agent::Error if client cert path is invalid") {
        agent_configuration.crt = "spam";

        REQUIRE_THROWS_AS(Agent { agent_configuration }, Agent::Error&);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    boost::filesystem::remove_all(SPOOL);
}

TEST_CASE("Agent::Agent without CRL", "[agent]") {
    Configuration::Agent agent_configuration { MODULES,
                                               TEST_BROKER_WS_URIS,
                                               std::vector<std::string> {},  // master uris
                                               "1",   // PCPv1
                                               getCaPath(),
                                               getCertPath(),
                                               getKeyPath(),
                                               "",
                                               SPOOL,
                                               "0d",  // don't purge spool!
                                               "",    // modules config dir
                                               "",    // task cache dir
                                               "0d",  // don't purge task cache!
                                               "test_agent",
                                               "",    // don't set broker proxy
                                               "",    // don't set master proxy
                                               5000, 10, 5, 5, 2, 15, 30, 120, 1024,
                                               leatherman::logging::log_level::none };

    SECTION("does not throw if it fails to find the external modules directory") {
        agent_configuration.modules_dir = MODULES + "/fake_dir";

        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    SECTION("should throw an Agent::Error if client cert path is invalid") {
        agent_configuration.crt = "spam";

        REQUIRE_THROWS_AS(Agent { agent_configuration }, Agent::Error&);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent { agent_configuration });
    }

    boost::filesystem::remove_all(SPOOL);
}
