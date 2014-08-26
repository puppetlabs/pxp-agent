#include "echo.h"
#include <valijson/constraints/concrete_constraints.hpp>

namespace CthunAgent {
namespace Modules {

Echo::Echo() {
    valijson::Schema input_schema;
    input_schema.addConstraint(new valijson::constraints::TypeConstraint(valijson::constraints::TypeConstraint::kString));

    valijson::Schema output_schema;
    output_schema.addConstraint(new valijson::constraints::TypeConstraint(valijson::constraints::TypeConstraint::kString));

    actions["echo"] = Action { input_schema, output_schema };
}

}  // namespace Modules
}  // namespace CthunAgent
