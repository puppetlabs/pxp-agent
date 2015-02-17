#include "src/modules/status.h"
#include "src/log.h"
#include "src/file_utils.h"
#include "src/configuration.h"

#include <valijson/constraints/concrete_constraints.hpp>

#include <fstream>

LOG_DECLARE_NAMESPACE("modules.status");

namespace CthunAgent {
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

DataContainer Status::callAction(const std::string& action_name,
                                 const ParsedContent& request) {
    DataContainer output {};
    auto request_input = request.data.get<DataContainer>("params");
    auto job_id = request_input.get<std::string>("job_id");

    auto spool_dir = Configuration::Instance().get<std::string>("spool-dir");

    if (!FileUtils::fileExists(spool_dir + job_id)) {
        LOG_ERROR("Found no results for job id %1%", job_id);
        output.set<std::string>("No job exists for id: " + job_id, "error");
        return output;
    }

    DataContainer status { FileUtils::readFileAsString(spool_dir +
                                                       job_id + "/status") };
    if (status.get<std::string>("status").compare("running") == 0) {
        output.set<std::string>("Running", "status");
    } else if (status.get<std::string>("status").compare("completed") == 0) {
        output.set<std::string>("Completed", "status");
        output.set<std::string>(FileUtils::readFileAsString(spool_dir +
                                                            job_id + "/stdout"),
                                "stdout");
        output.set<std::string>(FileUtils::readFileAsString(spool_dir +
                                                            job_id + "/stderr"),
                                "stderr");
    } else {
        output.set<std::string>("Job '" + job_id + "' is in unknown state:" +
                                status.get<std::string>("status"), "status");
    }

    return output;
}

}  // namespace Modules
}  // namespace CthunAgent
