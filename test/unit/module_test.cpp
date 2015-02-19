#include "test/test.h"

#include "src/message.h"
#include "src/data_container.h"
#include "src/errors.h"
#include "src/modules/echo.h"

#include <string>
#include <vector>

namespace CthunAgent {

static const std::string echo_action { "echo" };
static const std::string fake_action { "fake_action" };

static const std::string echo_data_txt {
    "{\"module\" : \"echo\","
    " \"action\" : \"echo\","
    " \"params\" : \"maradona\""
    "}"
};

static const std::vector<DataContainer> no_debug {};

static const ParsedContent content { DataContainer(),
                                     DataContainer(echo_data_txt),
                                     no_debug };

static const std::string bad_echo_data_txt {
    "{\"module\" : \"echo\","
    " \"action\" : \"echo\","
    " \"params\" : [1, 2, 3, 4 ,5]"
    "}"
};

static const ParsedContent bad_content { DataContainer(),
                                         DataContainer(bad_echo_data_txt),
                                         no_debug };

TEST_CASE("Module::validateAndCallAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("it should correctly call echo") {
        auto result = echo_module.validateAndCallAction(echo_action, content);
        REQUIRE(result.toString().find("maradona") != std::string::npos);
    }

    SECTION("it should throw a message_validation_error if the action is unknown") {
        REQUIRE_THROWS_AS(echo_module.validateAndCallAction(fake_action, content),
                          message_validation_error);
    }

    SECTION("it should throw a message_validation_error if the message is invalid") {
        REQUIRE_THROWS_AS(echo_module.validateAndCallAction(echo_action, bad_content),
                          message_validation_error);
    }
}

}  // namespace CthunAgent
