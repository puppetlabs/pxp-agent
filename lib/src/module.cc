#include <pxp-agent/module.hpp>
#include <pxp-agent/action_status.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.module"
#include <leatherman/logging/logging.hpp>

#include <boost/format.hpp>

#include <iostream>
#include <algorithm>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

Module::Module()
        : input_validator_ {},
          results_validator_ {}
{
}

bool Module::hasAction(const std::string& action_name)
{
    return std::find(actions.begin(), actions.end(), action_name)
           != actions.end();
}

void Module::validateOutputAndUpdateMetadata(ActionResponse& response)
{
    LOG_TRACE("Validating the results for the %1%", response.prettyRequestLabel());
    std::string err_msg {};

    try {
        results_validator_.validate(
            response.action_metadata.get<lth_jc::JsonContainer>("results"),
            response.action_metadata.get<std::string>("action"));
        LOG_TRACE("Successfully validated the results for the %1%",
                  response.prettyRequestLabel());
    } catch (PCPClient::validation_error) {
        // Modify the response metadata to indicate the failure
        if (type() == ModuleType::Internal) {
            // This is unexpected
            err_msg += "The task executed for the ";
            err_msg += response.prettyRequestLabel();
            err_msg += " returned invalid results.";
        } else {
            // Log about the output
            const auto& out = response.output.std_out;
            const auto& err = response.output.std_out;
            err_msg =
                (boost::format("The task executed for the %1% returned %2% "
                               "results on stdout%3% - stderr:%4%")
                    % response.prettyRequestLabel()
                    % (out.empty() ? "no" : "invalid")
                    % (out.empty() ? "" : ": " + out)
                    % (err.empty() ? " (empty)" : '\n' + err))
                .str();
        }

        LOG_DEBUG(err_msg);
        response.action_metadata.set<bool>("results_are_valid", false);
        response.action_metadata.set<std::string>("execution_error", err_msg);
        response.setStatus(ActionStatus::Failure);
    }
}

ActionResponse Module::executeAction(const ActionRequest& request)
{
    std::string err_msg {};

    try {
        auto response = callAction(request);
        assert(response.valid()
                && response.action_metadata.includes("results_are_valid"));

        if (!response.action_metadata.get<bool>("results_are_valid")) {
            // We expect that the action's output is not valid JSON
            assert(response.action_metadata.includes("execution_error"));
            return response;
        }

        assert(response.action_metadata.includes("results"));
        validateOutputAndUpdateMetadata(response);
        return response;
    } catch (const Module::ProcessingError& e) {
        err_msg += "Error: ";
        err_msg += e.what();
    } catch (std::exception& e) {
        err_msg += "Unexpected error: ";
        err_msg += e.what();
    } catch (...) {
        err_msg = "Unexpected excption.";
    }

    auto execution_error =
        (boost::format("Failed to execute the task for the %1%. %2%")
            % request.prettyLabel()
            % err_msg)
        .str();
    LOG_ERROR(execution_error);
    ActionResponse r { type(), request };
    r.setBadResultsAndEnd(execution_error);
    return r;
}

}  // namespace PXPAgent
