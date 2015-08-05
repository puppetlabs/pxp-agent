#include "../content_format.hpp"

#include <cthun-agent/modules/inventory.hpp>

#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <algorithm>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;

static const std::string INVENTORY_ACTION { "inventory" };

static const std::string INVENTORY_TXT {
    (DATA_FORMAT % "\"0367\""
                 % "\"inventory\""
                 % "\"inventory\""
                 % "{}").str() };

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};
static const CthunClient::ParsedChunks PARSED_CHUNKS {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(INVENTORY_TXT),
                    NO_DEBUG,
                    0 };

TEST_CASE("Modules::Inventory::callAction", "[modules]") {
    Modules::Inventory inventory_module {};
    ActionRequest request { RequestType::Blocking, PARSED_CHUNKS };

    SECTION("the inventory module is correctly named") {
        REQUIRE(inventory_module.module_name == "inventory");
    }

    SECTION("the inventory module has the inventory action") {
        auto found = std::find(inventory_module.actions.begin(),
                               inventory_module.actions.end(),
                               INVENTORY_ACTION);
        REQUIRE(found != inventory_module.actions.end());
    }

    SECTION("it can call the inventory action") {
        REQUIRE_NOTHROW(inventory_module.executeAction(request));
    }

    SECTION("it should execute the inventory action correctly") {
        auto outcome = inventory_module.executeAction(request);
        CHECK(outcome.results.toString().find("facts") != std::string::npos);
    }
}

}  // namespace CthunAgent
