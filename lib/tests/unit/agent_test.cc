#include "test/test.hpp"
#include <catch.hpp>

#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>

extern std::string ROOT_PATH;

namespace CthunAgent {

const std::string TEST_AGENT_IDENTITY { "cth://cthun-client/cthun-agent" };
const std::string TEST_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };

std::string getCa() {
    static const std::string ca {
        ROOT_PATH + "/lib/tests/resources/config/ca_crt.pem" };
    return ca;
}

std::string getCert() {
    static const std::string cert {
        ROOT_PATH + "/lib/tests/resources/config/test_crt.pem" };
    return cert;
}

std::string getKey() {
    static const std::string key {
        ROOT_PATH + "/lib/tests/resources/config/test_key.pem" };
    return key;
}

std::string getBin() {
    static const std::string bin {
        ROOT_PATH + "/bin" };
    return bin;
}

TEST_CASE("Agent::Agent", "[agent]") {
    SECTION("does not throw if it fails to find the modules directory") {
        REQUIRE_NOTHROW(Agent(getBin() + "/fake_dir", TEST_SERVER_URL,
                              getCa(), getCert(), getKey()));
    }

    SECTION("should throw a fatal_error if client cert path is invalid") {
        REQUIRE_THROWS_AS(Agent(getBin(), TEST_SERVER_URL,
                                getCa(), "spam", getKey()),
                          fatal_error);
    }

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(Agent(getBin(), TEST_SERVER_URL,
                              getCa(), getCert(), getKey()));
    }
}

}  // namespace CthunAgent
