#include <pxp-agent/module.hpp>

#include <iostream>
#include <algorithm>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.module"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

Module::Module()
        : input_validator_ {},
          results_validator_ {} {
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
            results_validator_.validate(outcome.results, request.action());
        } catch (PCPClient::validation_error) {
            std::string err_msg { "'" + module_name + " " + request.action()
                                  + "' returned an invalid result" };
            if (!outcome.std_err.empty()) {
                err_msg += " - stderr: " + outcome.std_err;
            }
            throw Module::ProcessingError { err_msg + outcome.std_err };
        }

        return outcome;
    } catch (Module::ProcessingError) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Faled to execute '%1% %2%': %3%",
                  module_name, request.action(), e.what());
        throw Module::ProcessingError { "failed to execute '" + module_name
                                        + " " + request.action() + "'" };
    } catch (...) {
        LOG_ERROR("Failed to execute '%1% %2%' - unexpected exception",
                  module_name, request.action());
        throw Module::ProcessingError { "failed to execute '" + module_name
                                        + " " + request.action() + "'" };
    }
}

}  // namespace PXPAgent
