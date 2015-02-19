#include "src/modules/inventory.h"
#include "src/log.h"
#include "src/errors.h"

#include <facter/facts/collection.hpp>

#include <valijson/constraints/concrete_constraints.hpp>

#include <sstream>

LOG_DECLARE_NAMESPACE("modules.inventory");

namespace CthunAgent {
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

DataContainer Inventory::callAction(const std::string& action_name,
                                    const ParsedContent& request) {
    std::ostringstream fact_stream;
    DataContainer data {};

    facter::facts::collection facts;
    facts.add_default_facts();
    facts.write(fact_stream, facter::facts::format::json);

    LOG_TRACE("facts: %1%", fact_stream.str());
    data.set<std::string>(fact_stream.str(), "facts");

    return data;
}

}  // namespace Modules
}  // namespace CthunAgent
