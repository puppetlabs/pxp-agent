#include "src/module.h"
#include "src/schemas.h"
#include "src/errors.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.module"
#include <leatherman/logging/logging.hpp>

#include <iostream>

namespace CthunAgent {

DataContainer Module::validateAndCallAction(const std::string& action_name,
                                            const ParsedContent& request) {
    // Validate action name
    if (actions.find(action_name) == actions.end()) {
        throw message_validation_error { "unknown action '" + action_name
                                         + "' for module " + module_name };
    }

    // Validate request input
    auto action = actions[action_name];
    auto request_input = request.data.get<DataContainer>("params");

    LOG_DEBUG("Validating input for '%1% %2%'", module_name, action_name);
    std::vector<std::string> errors;
    if (!request_input.validate(action.input_schema, errors)) {
        LOG_ERROR("Invalid input for '%1% %2%'", module_name, action_name);
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw message_validation_error { "invalid input for '" + module_name
                                         + " " + action_name + "'" };
    }

    DataContainer response_output;

    // Execute action
    try {
        response_output = callAction(action_name, request);
    } catch (message_error) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Faled to execute '%1% %2%': %3%",
                  module_name, action_name, e.what());
        throw message_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    } catch (...) {
        LOG_ERROR("Failed to execute '%1% %2%' - unexpected exception",
                  module_name, action_name);
        throw message_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    }

    if (!action.isDelayed()) {
        // Validate the result output
        LOG_DEBUG("Validating output for '%1% %2%'", module_name, action_name);
        if (!response_output.validate(action.output_schema, errors)) {
            LOG_ERROR("Invalid output from '%1% %2%'", module_name, action_name);
            for (auto error : errors) {
                LOG_ERROR("    %1%", error);
            }
            throw message_processing_error { "the action '" + module_name + " "
                                             + action_name + "' returned an "
                                             "invalid result" };
        }
    }

    return response_output;
}

}  // namespace CthunAgent
