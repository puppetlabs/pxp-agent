#include "src/agent/modules/inventory.h"
#include "src/common/log.h"

#include <facter/facts/collection.hpp>

#include <valijson/constraints/concrete_constraints.hpp>

#include <sstream>

LOG_DECLARE_NAMESPACE("agent.modules.inventory");

namespace Cthun {
namespace Agent {
namespace Modules {

Inventory::Inventory() {
    module_name = "inventory";

    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject
    };

    valijson::Schema input_schema;
    input_schema.addConstraint(json_type_object);

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_object);

    actions["inventory"] = Action { input_schema, output_schema, "interactive" };
}

DataContainer Inventory::call_action(std::string action_name,
                                     const Message& request,
                                     const DataContainer& input) {
    std::ostringstream fact_stream;
    DataContainer data {};

    try {
        facter::facts::collection facts;
        facts.add_default_facts();
        facts.write(fact_stream, facter::facts::format::json);
    } catch (...) {
        LOG_ERROR("failed to retrieve facts");
        data.set<std::string>("Failed to retrieve facts", "error");
        return data;
    }

    LOG_TRACE("facts: %1%", fact_stream.str());
    data.set<std::string>(fact_stream.str(), "facts");

    return data;
}

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun
