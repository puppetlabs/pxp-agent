#include <cthun-agent/modules/echo.hpp>
#include <cthun-agent/errors.hpp>

namespace CthunAgent {
namespace Modules {

static const std::string ECHO { "echo" };

Echo::Echo() {
    module_name = ECHO;

    // TODO(ale): revisit once we require the JSON format for all data
    CthunClient::Schema input_schema { ECHO };
    CthunClient::Schema output_schema { ECHO };

    actions[ECHO] = Action { "interactive" };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

CthunClient::DataContainer Echo::callAction(
                            const std::string& action_name,
                            const CthunClient::ParsedChunks& parsed_chunks) {
    auto params = parsed_chunks.data.get<CthunClient::DataContainer>("params");
    CthunClient::DataContainer result {};

    // TODO(ale): "argument" and "outcome" are a provisional fix;
    // make this compliant with C&C specs
    if (params.includes("argument")
        && params.type("argument") == CthunClient::DataType::String) {
        result.set<std::string>("outcome", params.get<std::string>("argument"));
    } else {
        throw request_processing_error { "bad argument type" };
    }

    return result;
}

}  // namespace Modules
}  // namespace CthunAgent
