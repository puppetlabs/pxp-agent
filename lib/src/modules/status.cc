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

    CthunClient::Schema input_schema { QUERY };
    CthunClient::Schema output_schema { QUERY };

    actions[QUERY] = Action { "interactive" };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

CthunClient::DataContainer Status::callAction(
                            const std::string& action_name,
                            const CthunClient::ParsedChunks& parsed_chunks) {
    CthunClient::DataContainer output {};
    auto request_input =
        parsed_chunks.data.get<CthunClient::DataContainer>("params");
    auto job_id = request_input.get<std::string>("job_id");

    auto spool_dir = Configuration::Instance().get<std::string>("spool-dir");

    if (!FileUtils::fileExists(spool_dir + job_id)) {
        LOG_ERROR("Found no results for job id %1%", job_id);
        output.set<std::string>("error", "No job exists for id: " + job_id);
        return output;
    }

    CthunClient::DataContainer status {
        FileUtils::readFileAsString(spool_dir + job_id + "/status") };

    auto status_txt = status.get<std::string>("status");
    if (status_txt == "running") {
        output.set<std::string>("status", "Running");
    } else if (status_txt == "completed") {
        output.set<std::string>("status", "Completed");
        output.set<std::string>("stdout",
                FileUtils::readFileAsString(spool_dir + job_id + "/stdout"));
        output.set<std::string>("stderr",
                FileUtils::readFileAsString(spool_dir + job_id + "/stderr"));
    } else {
        std::string tmp { "Job '" + job_id + "' is in unknown state: " };
        output.set<std::string>("status", tmp + status_txt);
    }

    return output;
}

}  // namespace Modules
}  // namespace CthunAgent
