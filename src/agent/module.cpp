#include "src/agent/module.h"
#include "src/agent/schemas.h"
#include "src/agent/errors.h"
#include "src/common/log.h"

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>

LOG_DECLARE_NAMESPACE("agent.module");

namespace Cthun {
namespace Agent {

void Module::call_action(std::string action_name, const Json::Value& input,
                         Json::Value& output) {
    LOG_INFO("invoking native action %1%", action_name);
}

void Module::validate_and_call_action(std::string action_name,
                                      const Json::Value& input,
                                      Json::Value& output,
                                      std::string action_id) {
    if (actions.find(action_name) == actions.end()) {
        throw validation_error { "unknown action for module " + module_name
                                 + ": '" + action_name + "'" };
    }

    const Action& action = actions[action_name];

    LOG_INFO("validating input for '%1% %2%'", module_name, action_name);
    std::vector<std::string> errors;
    if (!Schemas::validate(input, action.input_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "Input schema mismatch" };
    }

    if (action_id.empty()) {
        call_action(action_name, input, output);
    } else {
        call_delayed_action(action_name, input, output, action_id);
    }

    LOG_INFO("validating output for %1% %2%", module_name, action_name);
    if (!Schemas::validate(output, action.output_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "Output schema mismatch" };
    }
}

}  // namespace Agent
}  // namespace Cthun
