#include "test/test.h"

#include "src/data_container.h"
#include "src/agent/errors.h"
#include "src/agent/modules/inventory.h"

extern std::string ROOT_PATH;

namespace Cthun {
namespace Agent {

static const std::string inventory_action { "inventory" };
static const std::string inventory_txt =
    "{\"data\" : {"
    "    \"module\" : \"inventory\","
    "    \"action\" : \"inventory\","
    "    \"params\" : {}"
    "    }"
    "}";
static const Message msg { inventory_txt };
static const DataContainer input {};

// TEST_CASE("Agent::Modules::Inventory::call_action", "[modules]") {
//     Modules::Inventory inventory_module {};

//     SECTION("the inventory module is correctly named") {
//         REQUIRE(inventory_module.module_name == inventory_action);
//     }

//     SECTION("the inventory module has the inventory action") {
//         REQUIRE(inventory_module.actions.find(inventory_action)
//                 != inventory_module.actions.end());
//     }

//     SECTION("it can call the inventory action") {
//         REQUIRE_NOTHROW(inventory_module.call_action(inventory_action, msg, input));
//     }

//     SECTION("it 'should' execute the inventory action correctly") {
//         auto result = inventory_module.call_action(inventory_action, msg, input);
//         CHECK(result.toString().find("facts"));
//     }
// }

}  // namespace Agent
}  // namespace Cthun
