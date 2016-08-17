#include "../common/content_format.hpp"

#include <pxp-agent/modules/echo.hpp>
#include <pxp-agent/module_type.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <string>
#include <vector>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

static const std::string ECHO_ACTION { "echo" };
static const std::string FAKE_ACTION { "FAKE_ACTION" };

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};

static const std::string ECHO_TXT {
    (DATA_FORMAT % "\"0987\""
                 % "\"echo\""
                 % "\"echo\""
                 % "{ \"argument\" : \"maradona\" }").str() };

TEST_CASE("Module::type", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("correctly reports its type") {
        REQUIRE(echo_module.type() == ModuleType::Internal);
    }
}

TEST_CASE("Module::hasAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("correctly reports false") {
        REQUIRE(!echo_module.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(echo_module.hasAction(ECHO_ACTION));
    }
}

TEST_CASE("Module::executeAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("it should correctly call echo") {
        lth_jc::JsonContainer data { ECHO_TXT };
        ActionRequest request { RequestType::Blocking, MESSAGE_ID, SENDER, data };
        auto response = echo_module.executeAction(request);
        auto txt = response.action_metadata.get<std::string>({ "results", "outcome" });
        REQUIRE(txt == "maradona");
    }
}

}  // namespace PXPAgent
