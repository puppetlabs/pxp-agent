#include "echo.h"
#include <valijson/constraints/concrete_constraints.hpp>

namespace CthunAgent {
namespace Modules {

Echo::Echo() {
    valijson::constraints::TypeConstraint json_type_string(valijson::constraints::TypeConstraint::kString);

    valijson::Schema input_schema;
    input_schema.addConstraint(json_type_string);

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_string);

    actions["echo"] = Action { input_schema, output_schema };
}

}  // namespace Modules
}  // namespace CthunAgent
