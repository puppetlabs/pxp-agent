#include "test/test.h"

#include "src/errors.h"
#include "src/modules/inventory.h"

#include <string>
#include <vector>

extern std::string ROOT_PATH;

namespace CthunAgent {

// TODO(ale): update this to cthun-client

static const std::string inventory_action { "inventory" };
static const std::string inventory_txt {
    "{  \"module\" : \"inventory\","
    "   \"action\" : \"inventory\","
    "   \"params\" : {}"
    "}"
};

static const std::vector<DataContainer> no_debug {};
static const ParsedContent content { DataContainer(),
                                     DataContainer(inventory_txt),
                                     no_debug };

TEST_CASE("Modules::Inventory::callAction", "[modules]") {
    Modules::Inventory inventory_module {};

    SECTION("the inventory module is correctly named") {
        REQUIRE(inventory_module.module_name == "inventory");
    }

    SECTION("the inventory module has the inventory action") {
        REQUIRE(inventory_module.actions.find(inventory_action)
                != inventory_module.actions.end());
    }

    SECTION("it can call the inventory action") {
        REQUIRE_NOTHROW(inventory_module.callAction(inventory_action, content));
    }

    SECTION("it should execute the inventory action correctly") {
        auto result = inventory_module.callAction(inventory_action, content);
        CHECK(result.toString().find("facts") != std::string::npos);
    }
}

}  // namespace CthunAgent
