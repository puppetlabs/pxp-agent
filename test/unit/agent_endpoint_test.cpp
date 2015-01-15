#include "test/test.h"

#include "src/agent/agent_endpoint.h"
#include "src/agent/errors.h"
#include "src/websocket/errors.h"

namespace Cthun {
namespace Agent {

TEST_CASE("Agent::AgentEndpoint::AgentEndpoint", "[agent]") {
    SECTION("it should not throw if it fails to find the modules directory") {
        REQUIRE_NOTHROW(Agent::AgentEndpoint agent { "./../../bin/fake_dir" });
    }

    SECTION("it should correctly instantiate") {
        REQUIRE_NOTHROW(Agent::AgentEndpoint agent { "./../../bin" });
    }
}

static const std::string DEFAULT_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };
static std::string DEFAULT_CA { "./../../test-resources/ssl/ca/ca_crt.pem" };
static std::string DEFAULT_CERT {
    "./../../test-resources/ssl/certs/cthun-client.pem" };
static std::string DEFAULT_KEY {
    "./../../test-resources/ssl/private_keys/cthun-client.pem" };

TEST_CASE("Agent::AgentEndpoint::startAgent", "[agent]") {
    Agent::AgentEndpoint agent { "./../../bin" };

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

}  // namespace Agent
}  // namespace Cthun
