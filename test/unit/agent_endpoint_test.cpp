#include "test/test.h"

#include "src/agent_endpoint.h"
#include "src/errors.h"
#include "src/websocket/errors.h"

extern std::string ROOT_PATH;

namespace CthunAgent {

TEST_CASE("AgentEndpoint::AgentEndpoint", "[agent]") {
    SECTION("it should not throw if it fails to find the modules directory") {
        REQUIRE_NOTHROW(AgentEndpoint agent { ROOT_PATH + "/bin/fake_dir" });
    }

    SECTION("it should correctly instantiate") {
        REQUIRE_NOTHROW(AgentEndpoint agent { ROOT_PATH + "/bin" });
    }
}

static const std::string DEFAULT_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };
static std::string DEFAULT_CA { ROOT_PATH + "/test-resources/ssl/ca/ca_crt.pem" };
static std::string DEFAULT_CERT {
    ROOT_PATH + "/test-resources/ssl/certs/cthun-client.pem" };
static std::string DEFAULT_KEY {
    ROOT_PATH + "/test-resources/ssl/private_keys/cthun-client.pem" };

TEST_CASE("AgentEndpoint::startAgent", "[agent]") {
    AgentEndpoint agent { ROOT_PATH + "/bin" };

    SECTION("should throw a fatal_error if client cert is invalid") {
        REQUIRE_THROWS_AS(agent.startAgent(DEFAULT_SERVER_URL, DEFAULT_CA,
                                           "spam", DEFAULT_KEY),
                          fatal_error);
    }

    SECTION("should throw a fatal_error if the server url is invalid") {
        REQUIRE_THROWS_AS(agent.startAgent("foo", DEFAULT_CA, DEFAULT_CERT,
                                           DEFAULT_KEY),
                          fatal_error);
    }
}

}  // namespace CthunAgent
