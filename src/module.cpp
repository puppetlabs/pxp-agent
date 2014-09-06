#include "module.h"
#include "schemas.h"

#include <cthun-client/src/log/log.h>

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>

LOG_DECLARE_NAMESPACE("agent.module");

namespace CthunAgent {

void Module::call_action(std::string action, const Json::Value& input,
                         Json::Value& output) {
    LOG_INFO("invoking native action %1%", action);
}

void Module::validate_and_call_action(std::string action,
                                      const Json::Value& input,
                                      Json::Value& output) {
    const Action& action_to_invoke = actions[action];

    LOG_INFO("validating input for %1% %2%", name, action);
    std::vector<std::string> errors;
    if (!Schemas::validate(input, action_to_invoke.input_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw "input schema mismatch";
    }

    call_action(action, input, output);

    LOG_INFO("validating output for %1% %2%", name, action);
    if (!Schemas::validate(output, action_to_invoke.output_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw "output schema mismatch";
    }

    LOG_INFO("validated OK: %1%", output.toStyledString());
}

}  // namespace CthunAgent
