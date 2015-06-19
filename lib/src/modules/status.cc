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
    auto spool_dir = Configuration::Instance().get<std::string>("spool-dir");

    if (!FileUtils::fileExists(spool_dir + job_id)) {
        // TODO(ale): return "unknown" in this case; see RPC specs

        LOG_ERROR("Found no results for job id %1%", job_id);
        results.set<std::string>("error", "No job exists for id: " + job_id);
        return results;
    }

    CthunClient::DataContainer status {
        FileUtils::readFileAsString(spool_dir + job_id + "/status") };

    auto status_txt = status.get<std::string>("status");
    if (status_txt == "running") {
        results.set<std::string>("status", "Running");
    } else if (status_txt == "completed") {
        results.set<std::string>("status", "Completed");
        results.set<std::string>("stdout",
                    FileUtils::readFileAsString(spool_dir + job_id + "/stdout"));
        results.set<std::string>("stderr",
                    FileUtils::readFileAsString(spool_dir + job_id + "/stderr"));
    } else {
        std::string tmp { "Job '" + job_id + "' is in unknown state: " };
        results.set<std::string>("status", tmp + status_txt);
    }

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
