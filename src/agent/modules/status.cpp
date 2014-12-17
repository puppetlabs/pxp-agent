#include "src/agent/modules/status.h"
#include "src/common/log.h"
#include "src/common/file_utils.h"

#include <valijson/constraints/concrete_constraints.hpp>

#include <fstream>

LOG_DECLARE_NAMESPACE("agent.modules.status");

namespace Cthun {
namespace Agent {
namespace Modules {

Status::Status() {
    module_name = "status";

    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject
    };

    valijson::Schema input_schema;
    input_schema.addConstraint(json_type_object);

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_object);

    actions["query"] = Action { input_schema, output_schema, "interactive" };
}

DataContainer Status::call_action(std::string action_name,
                         const Message& request,
                         const DataContainer& input) {

    DataContainer output {};
    std::string job_id { input.get<std::string>("job_id") };

    if (!Common::FileUtils::fileExists("/tmp/cthun_agent/" + job_id)) {
        LOG_ERROR("No results for job id %1% found", job_id);
        output.set<std::string>("No job exists for id: " + job_id, "error");
        return output;
    }

    DataContainer status { Common::FileUtils::readFileAsString("/tmp/cthun_agent/" +
                                                      job_id +
                                                      "/status") };
    if (status.get<std::string>("status").compare("running") == 0) {
        output.set<std::string>("Running", "status");
    } else if (status.get<std::string>("status").compare("completed") == 0) {
        output.set<std::string>("Running", "status");
        output.set<std::string>(Common::FileUtils::readFileAsString("/tmp/cthun_agent/" +
                                                                    job_id +
                                                                    "/stdout"),
                                "stdout");
        output.set<std::string>(Common::FileUtils::readFileAsString("/tmp/cthun_agent/" +
                                                                    job_id +
                                                                    "/stderr"),
                                "stderr");
    } else {
        output.set<std::string>("Job '" + job_id + "' is in unknown state:" +
                                status.get<std::string>("status"), "status");
    }

    return output;
}

}  // namespace Modules
}  // namespace Agent
}  // namespace Cthun
