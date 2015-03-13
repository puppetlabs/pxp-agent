#include "src/modules/echo.h"

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
    return parsed_chunks.data.get<CthunClient::DataContainer>("params");
}

}  // namespace Modules
}  // namespace CthunAgent
