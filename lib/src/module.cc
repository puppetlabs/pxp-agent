#include <cthun-agent/module.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/file_utils.hpp>

#include <iostream>
#include <algorithm>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.module"
#include <leatherman/logging/logging.hpp>

namespace CthunAgent {

Module::Module()
        : input_validator_ {},
          output_validator_ {} {
}

bool Module::hasAction(const std::string& action_name) {
    return std::find(actions.begin(), actions.end(), action_name)
           != actions.end();
}

ActionOutcome Module::executeAction(const ActionRequest& request) {
    auto& action_name = request.action();
    assert(hasAction(action_name));

    // Validate request input
    try {
        LOG_DEBUG("Validating input for '%1% %2%'", module_name, action_name);
        // NB: the registred schemas have the same name as the action

        // TODO(ale): try to make const the DataContainer argument of
        // CthunClient::Validator::validate() to avoid copying

        CthunClient::DataContainer input_params { request.params() };

        input_validator_.validate(input_params, action_name);
    } catch (CthunClient::validation_error) {
        throw request_validation_error { "invalid input for '" + module_name
                                         + " " + action_name + "'" };
    }

    try {
        // Execute action
        auto outcome = callAction(request);

        // Validate action output
        LOG_DEBUG("Validating the result output for '%1% %2%'",
                  module_name, action_name);
        try {
            output_validator_.validate(outcome.results, action_name);
        } catch (CthunClient::validation_error) {
            std::string err_msg { "'" + module_name + " " + action_name + "' "
                                  "returned an invalid result - stderr: " };
            throw request_processing_error { err_msg + outcome.stderr };
        }

        return outcome;
    } catch (request_processing_error) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Faled to execute '%1% %2%': %3%",
                  module_name, action_name, e.what());
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    } catch (...) {
        LOG_ERROR("Failed to execute '%1% %2%' - unexpected exception",
                  module_name, action_name);
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    }
}

}  // namespace CthunAgent
