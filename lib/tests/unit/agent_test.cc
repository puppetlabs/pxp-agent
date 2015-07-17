#include "certs.hpp"

#include "root_path.hpp"

#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>

#include <catch.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

namespace CthunAgent {

const std::string TEST_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };
const std::string BIN_PATH { std::string { CTHUN_AGENT_ROOT_PATH } + "/bin" };
const std::string SPOOL { std::string { CTHUN_AGENT_ROOT_PATH }
                          + "/lib/tests/resources/tmp/" };

TEST_CASE("Agent::Agent", "[agent]") {
    SECTION("does not throw if it fails to find the external modules directory") {
        REQUIRE_NOTHROW(Agent(BIN_PATH + "/fake_dir", TEST_SERVER_URL,
                              getCaPath(), getCertPath(), getKeyPath(), SPOOL));
    }

    SECTION("should throw an Agent::Error if client cert path is invalid") {
        REQUIRE_THROWS_AS(Agent(BIN_PATH, TEST_SERVER_URL, getCaPath(), "spam",
                                getKeyPath(), SPOOL),
                          Agent::Error);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent(BIN_PATH, TEST_SERVER_URL, getCaPath(),
                              getCertPath(), getKeyPath(), SPOOL));
    }

    boost::filesystem::remove_all(SPOOL);
}

}  // namespace CthunAgent
