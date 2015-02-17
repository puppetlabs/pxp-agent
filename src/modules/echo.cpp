#include "src/modules/echo.h"

#include <valijson/constraints/concrete_constraints.hpp>

namespace CthunAgent {
namespace Modules {

Echo::Echo() {
    module_name = "echo";

    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };

    valijson::Schema input_schema;
    input_schema.addConstraint(json_type_string);

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_string);

    actions["echo"] = Action { input_schema, output_schema, "interactive" };
}

DataContainer Echo::callAction(const std::string& action_name,
                               const ParsedContent& request) {
    return request.data.get<DataContainer>("params");
}

}  // namespace Modules
}  // namespace CthunAgent
