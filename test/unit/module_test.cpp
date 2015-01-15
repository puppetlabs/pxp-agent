#include "test/test.h"

#include "src/data_container.h"
#include "src/agent/errors.h"
#include "src/agent/modules/echo.h"

namespace Cthun {
namespace Agent {

static const std::string echo_action { "echo" };
static const std::string fake_action { "fake_action" };
static const std::string echo_txt =
    "{\"data\" : {"
    "    \"module\" : \"echo\","
    "    \"action\" : \"echo\","
    "    \"params\" : \"maradona\""
    "    }"
    "}";
static const Message msg { echo_txt };
static const std::string bad_echo =
    "{\"data\" : {"
    "    \"module\" : \"echo\","
    "    \"action\" : \"echo\","
    "    \"params\" : [1, 2, 3, 4 ,5]"
    "    }"
    "}";

TEST_CASE("Agent::Module::validate_and_call_action", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("it should correctly call echo") {
        auto result = echo_module.validate_and_call_action(echo_action, msg);
        REQUIRE(result.toString().find("maradona"));
    }

    SECTION("it should throw a validation_error if the action is unknown") {
        REQUIRE_THROWS_AS(echo_module.validate_and_call_action(fake_action, msg),
                          validation_error);
    }

    SECTION("it should throw a validation_error if the message is invalid") {
        Message bad_msg { bad_echo };
        REQUIRE_THROWS_AS(echo_module.validate_and_call_action(echo_action, bad_msg),
                          validation_error);
    }
}

}  // namespace Agent
}  // namespace Cthun
