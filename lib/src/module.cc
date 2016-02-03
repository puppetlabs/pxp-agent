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
        LOG_DEBUG("Validating the result output for the %1%", request.prettyLabel());
        try {
            results_validator_.validate(outcome.results, request.action());
        } catch (PCPClient::validation_error) {
            throw Module::ProcessingError {
                (boost::format("The task executed for the %1% returned %2% "
                               "results on stdout%3% - stderr:%4%")
                    % request.prettyLabel()
                    % (outcome.std_out.empty() ? "no" : "invalid")
                    % (outcome.std_out.empty() ? "" : ": " + outcome.std_out)
                    % (outcome.std_err.empty() ? " (empty)" : '\n' + outcome.std_err))
                .str() };
        }

        return outcome;
    } catch (Module::ProcessingError) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Failed to execute the task for the %1%: %2%",
                  request.prettyLabel(), e.what());
        throw Module::ProcessingError { "failed to execute " + request.prettyLabel() };
    } catch (...) {
        LOG_ERROR("Failed to execute the task for the %1% - unexpected exception",
                  request.prettyLabel());
        throw Module::ProcessingError { "failed to execute " + request.prettyLabel() };
    }
}

}  // namespace PXPAgent
