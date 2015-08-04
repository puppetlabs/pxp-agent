#include "content_format.hpp"

#include <cthun-agent/modules/echo.hpp>

#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <string>
#include <vector>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;

static const std::string ECHO_ACTION { "echo" };
static const std::string FAKE_ACTION { "FAKE_ACTION" };

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};

static const std::string ECHO_TXT {
    (DATA_FORMAT % "\"0987\""
                 % "\"echo\""
                 % "\"echo\""
                 % "{ \"argument\" : \"maradona\" }").str() };

static const CthunClient::ParsedChunks PARSED_CHUNKS {
                lth_jc::JsonContainer(ENVELOPE_TXT),
                lth_jc::JsonContainer(ECHO_TXT),
                NO_DEBUG,
                0 };

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
        ActionRequest request { RequestType::Blocking, PARSED_CHUNKS };
        auto outcome = echo_module.executeAction(request);
        auto txt = outcome.results.get<std::string>("outcome");
        REQUIRE(txt == "maradona");
    }
}

}  // namespace CthunAgent
