#include "certs.hpp"

#include "root_path.hpp"

#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>

#include <catch.hpp>

namespace CthunAgent {

const std::string TEST_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };
const std::string BIN_PATH { std::string { CTHUN_AGENT_ROOT_PATH } + "/bin" };

TEST_CASE("Agent::Agent", "[agent]") {
    SECTION("does not throw if it fails to find the external modules directory") {
        REQUIRE_NOTHROW(Agent(BIN_PATH + "/fake_dir", TEST_SERVER_URL,
                              getCaPath(), getCertPath(), getKeyPath()));
    }

    SECTION("should throw a fatal_error if client cert path is invalid") {
        REQUIRE_THROWS_AS(Agent(BIN_PATH, TEST_SERVER_URL,
                                getCaPath(), "spam", getKeyPath()),
                          fatal_error);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent(BIN_PATH, TEST_SERVER_URL,
                              getCaPath(), getCertPath(), getKeyPath()));
    }
}

}  // namespace CthunAgent
