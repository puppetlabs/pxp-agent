#include "test/test.hpp"
#include <catch.hpp>

#include <cthun-agent/errors.hpp>
#include <cthun-agent/modules/echo.hpp>

#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <string>
#include <vector>

namespace CthunAgent {

static const std::string echo_action { "echo" };
static const std::string fake_action { "fake_action" };

static const std::string echo_data_txt {
    "{  \"module\" : \"echo\","
    "   \"action\" : \"echo\","
    "   \"params\" : { \"argument\" : \"maradona\" }"
    "}"
};

static const std::vector<CthunClient::DataContainer> no_debug {};

static const CthunClient::ParsedChunks parsed_chunks {
                CthunClient::DataContainer(),
                CthunClient::DataContainer(echo_data_txt),
                no_debug,
                0 };

static const std::string bad_echo_data_txt {
    "{  \"module\" : \"echo\","
    "   \"action\" : \"echo\","
    "   \"params\" : [1, 2, 3, 4 ,5]"
    "}"
};

static const CthunClient::ParsedChunks bad_parsed_chunks {
                CthunClient::DataContainer(),
                CthunClient::DataContainer(bad_echo_data_txt),
                no_debug,
                0 };

TEST_CASE("Module::hasAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("correctly reports false") {
        REQUIRE(!echo_module.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(echo_module.hasAction(echo_action));
    }
}

TEST_CASE("Module::executeAction", "[modules]") {
    Modules::Echo echo_module {};

    SECTION("it should correctly call echo") {
        ActionRequest request { RequestType::Blocking, parsed_chunks };
        auto outcome = echo_module.executeAction(request);
        auto txt = outcome.results.get<std::string>("outcome");
        REQUIRE(txt == "maradona");
    }
}

}  // namespace CthunAgent
