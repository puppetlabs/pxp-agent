#include "src/module.h"
#include "src/schemas.h"
#include "src/errors.h"
#include "src/log.h"

#include <iostream>

LOG_DECLARE_NAMESPACE("module");

namespace CthunAgent {

DataContainer Module::validateAndCallAction(const std::string& action_name,
                                            const Message& request) {
    // Validate action name
    if (actions.find(action_name) == actions.end()) {
        throw validation_error { "unknown action '" + action_name
                                 + "' for module " + module_name };
    }

    // Validate request input
    auto action = actions[action_name];
    DataContainer request_input { request.get<DataContainer>("data", "params") };

    LOG_DEBUG("Validating input for '%1%' '%2%'", module_name, action_name);
    std::vector<std::string> errors;
    if (!request_input.validate(action.input_schema, errors)) {
        LOG_ERROR("action input validation failed '%1%' '%2%'",
                  module_name, action_name);
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "input schema mismatch" };
    }

    // Execute action and validate the result output
    auto result = callAction(action_name, request);

    LOG_DEBUG("Validating output for '%1%' '%2%'", module_name, action_name);
    if (!result.validate(action.output_schema, errors)) {
        LOG_ERROR("Output validation failed '%1%' '%2%'", module_name, action_name);
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }

        throw validation_error { "output schema mismatch" };
    }

    return result;
}

}  // namespace CthunAgent
