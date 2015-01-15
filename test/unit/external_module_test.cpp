#include "test/test.h"

#include "src/data_container.h"
#include "src/agent/errors.h"
#include "src/agent/external_module.h"

extern std::string ROOT_PATH;

namespace Cthun {
namespace Agent {

TEST_CASE("Agent::ExternalModule::ExternalModule", "[modules]") {
    SECTION("can successfully instantiate from a valid external module") {
        REQUIRE_NOTHROW(ExternalModule(ROOT_PATH + "/test/unit/test_modules/reverse_valid"));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { ROOT_PATH + "/test/unit/test_modules/reverse_valid" };
        REQUIRE(mod.actions.size() == 2);
    }

    SECTION("throws an error in case of invalid overall metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(ROOT_PATH + "/test/unit/test_modules/reverse_broken_01"),
            module_error);
    }
}

static const std::string string_action { "string" };
static const std::string fake_action { "fake_action" };
static const std::string reverse_txt =
    "{\"data\" : {"
    "    \"module\" : \"reverse\","
    "    \"action\" : \"string\","
    "    \"params\" : \"maradona\""
    "    }"
    "}";
static const Message msg { reverse_txt };
static const std::string bad_reverse =
    "{\"data\" : {"
    "    \"module\" : \"reverse\","
    "    \"action\" : \"string\","
    "    \"params\" : [1, 2, 3, 4 ,5]"
    "    }"
    "}";

TEST_CASE("Agent::ExternalModule::validate_and_call_action", "[modules]") {
    ExternalModule reverse_module { ROOT_PATH + "/modules/reverse" };

    SECTION("it should correctly call the shipped reverse module") {
        auto result = reverse_module.validate_and_call_action(string_action, msg);
        REQUIRE(result.toString().find("anodaram"));
    }

    SECTION("it should throw a validation_error if the action is unknown") {
        REQUIRE_THROWS_AS(reverse_module.validate_and_call_action(fake_action, msg),
                          validation_error);
    }

    SECTION("it should throw a validation_error if the message is invalid") {
        Message bad_msg { bad_reverse };
        REQUIRE_THROWS_AS(reverse_module.validate_and_call_action(string_action,
                                                                  bad_msg),
                          validation_error);
    }
}

// TODO(ale): delayed action test

}  // namespace Agent
}  // namespace Cthun
