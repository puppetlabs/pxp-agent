#include "src/module.h"
#include "src/schemas.h"
#include "src/errors.h"
#include "src/log.h"

#include <iostream>

LOG_DECLARE_NAMESPACE("module");

namespace CthunAgent {

// TODO(ale): change 'params' to 'input' for consistency

DataContainer Module::validate_and_call_action(std::string action_name,
                                               const Message& request,
                                               std::string action_id) {
    if (actions.find(action_name) == actions.end()) {
        throw validation_error { "unknown action for module " + module_name
                                 + ": '" + action_name + "'" };
    }

    const Action& action = actions[action_name];
    DataContainer input { request.get<DataContainer>("data", "params") };

    LOG_DEBUG("validating input for '%1%' '%2%'", module_name, action_name);
    std::vector<std::string> errors;
    if (!input.validate(action.input_schema, errors)) {
        LOG_ERROR("action input validation failed '%1%' '%2%'",
                  module_name, action_name);
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "Input schema mismatch" };
    }

    DataContainer result;

    // TODO(ploubser): This still isn't great. I would like the logic in
    // call_delayed_action to be moved to the Agent::delayedActionThread.
    if (action_id.empty()) {
        result = call_action(action_name, request);
    } else {
        call_delayed_action(action_name, request, action_id);
    }

    LOG_DEBUG("validating output for '%1%' '%2%'", module_name, action_name);
    if (!result.validate(action.output_schema, errors)) {
        LOG_ERROR("output validation failed '%1%' '%2%'", module_name, action_name);
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "Output schema mismatch" };
    }

    return result;
}

}  // namespace CthunAgent
