#include <cthun-agent/modules/echo.hpp>
#include <cthun-agent/errors.hpp>

namespace CthunAgent {
namespace Modules {

static const std::string ECHO { "echo" };

Echo::Echo() {
    module_name = ECHO;
    actions.push_back(ECHO);
    CthunClient::Schema input_schema { ECHO };
    CthunClient::Schema output_schema { ECHO };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Echo::callAction(const std::string& action_name,
                               const CthunClient::ParsedChunks& parsed_chunks) {
    auto params = parsed_chunks.data.get<CthunClient::DataContainer>("params");
    CthunClient::DataContainer results {};

    if (params.includes("argument")
        && params.type("argument") == CthunClient::DataType::String) {
        results.set<std::string>("outcome", params.get<std::string>("argument"));
    } else {
        throw request_processing_error { "bad argument type" };
    }

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
