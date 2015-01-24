#include "test/test.h"

#include "src/data_container.h"
#include "src/errors.h"
#include "src/modules/echo.h"

namespace CthunAgent {

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

TEST_CASE("Module::validateAndCallAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("it should correctly call echo") {
        auto result = echo_module.validateAndCallAction(echo_action, msg);
        REQUIRE(result.toString().find("maradona"));
    }

    SECTION("it should throw a message_validation_error if the action is unknown") {
        REQUIRE_THROWS_AS(echo_module.validateAndCallAction(fake_action, msg),
                          message_validation_error);
    }

    SECTION("it should throw a message_validation_error if the message is invalid") {
        Message bad_msg { bad_echo };
        REQUIRE_THROWS_AS(echo_module.validateAndCallAction(echo_action, bad_msg),
                          message_validation_error);
    }
}

}  // namespace CthunAgent
