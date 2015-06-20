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
    try {
        // Execute action
        auto outcome = callAction(request);

        // Validate action output
        LOG_DEBUG("Validating the result output for '%1% %2%'",
                  module_name, request.action());
        try {
            output_validator_.validate(outcome.results, request.action());
        } catch (CthunClient::validation_error) {
            std::string err_msg { "'" + module_name + " " + request.action()
                                  + "' returned an invalid result - stderr: " };
            throw request_processing_error { err_msg + outcome.stderr };
        }

        return outcome;
    } catch (request_processing_error) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Faled to execute '%1% %2%': %3%",
                  module_name, request.action(), e.what());
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + request.action() + "'" };
    } catch (...) {
        LOG_ERROR("Failed to execute '%1% %2%' - unexpected exception",
                  module_name, request.action());
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + request.action() + "'" };
    }
}

}  // namespace CthunAgent
