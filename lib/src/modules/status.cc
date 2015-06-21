#include <cthun-agent/modules/status.hpp>
#include <cthun-agent/file_utils.hpp>
#include <cthun-agent/configuration.hpp>
#include <cthun-agent/errors.hpp>

#include <fstream>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.modules.status"
#include <leatherman/logging/logging.hpp>

namespace CthunAgent {
namespace Modules {

static const std::string QUERY { "query" };

Status::Status() {
    module_name = "status";
    actions.push_back(QUERY);
    CthunClient::Schema input_schema { QUERY };
    CthunClient::Schema output_schema { QUERY };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Status::callAction(const ActionRequest& request) {
    CthunClient::DataContainer results {};
    auto job_id = request.params().get<std::string>("job_id");
    auto results_dir = Configuration::Instance().get<std::string>("spool-dir")
                       + job_id;

    if (!FileUtils::fileExists(results_dir)) {
        LOG_ERROR("Found no results for job id %1%", job_id);
        results.set<std::string>("status", "Unknown");
    } else {
        LOG_DEBUG("Retrieving results for job id %1% from %2%", job_id, results_dir);
        CthunClient::DataContainer status {
            FileUtils::readFileAsString(results_dir + "/status") };

        auto status_txt = status.get<std::string>("status");
        if (status_txt == "running") {
            results.set<std::string>("status", "Running");
        } else if (status_txt == "completed") {
            results.set<std::string>("status", "Completed");
            results.set<std::string>("stdout",
                        FileUtils::readFileAsString(results_dir + "/stdout"));
            results.set<std::string>("stderr",
                        FileUtils::readFileAsString(results_dir + "/stderr"));
        } else {
            std::string tmp { "Job '" + job_id + "' is in unknown state: " };
            results.set<std::string>("status", tmp + status_txt);
        }
    }

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
