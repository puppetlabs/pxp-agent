#include "test/test.h"

#include "src/agent.h"
#include "src/errors.h"

extern std::string ROOT_PATH;

namespace CthunAgent {

const std::string TEST_AGENT_IDENTITY { "cth://cthun-client/cthun-agent" };
const std::string TEST_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };

std::string getCa() {
    static const std::string ca {
        ROOT_PATH + "/test-resources/ssl/ca/ca_crt.pem" };
    return ca;
}

std::string getCert() {
    static const std::string cert {
        ROOT_PATH + "/test-resources/ssl/certs/cthun-client.pem" };
    return cert;
}

std::string getKey() {
    static const std::string key {
        ROOT_PATH + "/test-resources/ssl/private_keys/cthun-client.pem" };
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

TEST_CASE("Agent::startAgent", "[agent]") {
    SECTION("should throw a fatal_error if the server url is invalid") {
        Agent agent { getBin(), "foo", getCa(), getCert(), getKey() };
        REQUIRE_THROWS_AS(agent.start(),
                          fatal_error);
    }
}

}  // namespace CthunAgent
