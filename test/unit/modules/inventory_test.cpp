#include <cstdio>

#include "test/test.h"

#include "src/data_container.h"
#include "src/errors.h"
#include "src/modules/inventory.h"

extern std::string ROOT_PATH;

namespace CthunAgent {

static const std::string inventory_action { "inventory" };
static const std::string inventory_txt =
    "{\"data\" : {"
    "    \"module\" : \"inventory\","
    "    \"action\" : \"inventory\","
    "    \"params\" : {}"
    "    }"
    "}";
static const Message msg { inventory_txt };

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
        REQUIRE_NOTHROW(inventory_module.callAction(inventory_action, msg));
    }

    SECTION("it should execute the inventory action correctly") {
        auto result = inventory_module.callAction(inventory_action, msg);
        CHECK(result.toString().find("facts"));
    }
}

}  // namespace CthunAgent
