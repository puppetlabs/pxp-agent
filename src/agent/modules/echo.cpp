#include "agent/modules/echo.h"

#include <valijson/constraints/concrete_constraints.hpp>

namespace Cthun {
namespace Agent {
namespace Modules {

Echo::Echo() {
    module_name = "echo";

    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };

    valijson::Schema input_schema;
    input_schema.addConstraint(json_type_string);

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_string);

    actions["echo"] = Action { input_schema, output_schema };
}

void Echo::call_action(std::string action_name, const Json::Value& input,
                       Json::Value& output) {
    output = Json::Value { input.asString() };
}

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun
