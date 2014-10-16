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

void Inventory::call_action(std::string action_name, const Json::Value& input,
                            Json::Value& output) {
    std::ostringstream fact_stream;

    try {
        facter::facts::collection facts;
        facts.add_default_facts();
        facts.write(fact_stream, facter::facts::format::json);
    } catch (...) {
        LOG_ERROR("failed to retrieve facts");
        Json::Value err_result;
        err_result["error"] = "Failed to retrieve facts";
        output = err_result;
        return;
    }

    LOG_TRACE("facts: %1%", fact_stream.str());

    // Parse the facts string (json) and copy into output
    Json::Reader reader;
    if (!reader.parse(fact_stream.str(), output["facts"])) {
        // Unexpected
        LOG_ERROR("json decode of facts failed");
        Json::Value err_result;
        err_result["error"] = "Failed to parse facts";
        output = err_result;
    }
}

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun
