#include <pxp-agent/modules/echo.hpp>

namespace PXPAgent {
namespace Modules {

static const std::string ECHO { "echo" };

Echo::Echo() {
    module_name = ECHO;
    actions.push_back(ECHO);
    PCPClient::Schema input_schema { ECHO };
    input_schema.addConstraint("argument", PCPClient::TypeConstraint::String,
                               true);
    PCPClient::Schema output_schema { ECHO };

    input_validator_.registerSchema(input_schema);
    results_validator_.registerSchema(output_schema);
}

ActionOutcome Echo::callAction(const ActionRequest& request) {
    auto params = request.params();

    assert(params.includes("argument")
           && params.type("argument") == lth_jc::DataType::String);

    lth_jc::JsonContainer results {};
    results.set<std::string>("outcome", params.get<std::string>("argument"));

    return ActionOutcome { EXIT_SUCCESS, results };
}

}  // namespace Modules
}  // namespace PXPAgent
