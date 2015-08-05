#include <cthun-agent/modules/echo.hpp>

namespace CthunAgent {
namespace Modules {

static const std::string ECHO { "echo" };

Echo::Echo() {
    module_name = ECHO;
    actions.push_back(ECHO);
    CthunClient::Schema input_schema { ECHO };
    input_schema.addConstraint("argument", CthunClient::TypeConstraint::String,
                               true);
    CthunClient::Schema output_schema { ECHO };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Echo::callAction(const ActionRequest& request) {
    auto params = request.params();

    assert(params.includes("argument")
           && params.type("argument") == lth_jc::DataType::String);

    lth_jc::JsonContainer results {};
    results.set<std::string>("outcome", params.get<std::string>("argument"));

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
