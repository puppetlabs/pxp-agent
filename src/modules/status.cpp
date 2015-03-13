#include "src/modules/status.h"
#include "src/log.h"
#include "src/file_utils.h"
#include "src/configuration.h"
#include "src/errors.h"

#include <fstream>

LOG_DECLARE_NAMESPACE("modules.status");

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
        output.set<std::string>("No job exists for id: " + job_id, "error");
        return output;
    }

    CthunClient::DataContainer status {
        FileUtils::readFileAsString(spool_dir + job_id + "/status") };

    auto status_txt = status.get<std::string>("status");
    if (status_txt == "running") {
        output.set<std::string>("Running", "status");
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
