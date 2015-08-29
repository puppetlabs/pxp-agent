#include <pxp-agent/modules/status.hpp>
#include <pxp-agent/configuration.hpp>
#include <boost/filesystem.hpp>

#include <leatherman/file_util/file.hpp>

#include <fstream>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.modules.status"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace lth_file = leatherman::file_util;

static const std::string QUERY { "query" };

Status::Status() {
    module_name = "status";
    actions.push_back(QUERY);
    PCPClient::Schema input_schema { QUERY };
    input_schema.addConstraint("job_id", PCPClient::TypeConstraint::String,
                               true);

    PCPClient::Schema output_schema { QUERY };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Status::callAction(const ActionRequest& request) {
    lth_jc::JsonContainer results {};
    auto job_id = request.params().get<std::string>("job_id");
    auto results_dir = Configuration::Instance().get<std::string>("spool-dir")
                       + job_id;

    if (!boost::filesystem::exists(results_dir)) {
        LOG_ERROR("Found no results for job id %1%", job_id);
        results.set<std::string>("status", "Unknown");
    } else {
        LOG_DEBUG("Retrieving results for job id %1% from %2%", job_id, results_dir);
        lth_jc::JsonContainer status {
                lth_file::read(results_dir + "/status") };

        auto status_txt = status.get<std::string>("status");
        if (status_txt == "running") {
            results.set<std::string>("status", "Running");
        } else if (status_txt == "completed") {
            results.set<std::string>("status", "Completed");
            results.set<std::string>("stdout",
                                     lth_file::read(results_dir + "/stdout"));
            results.set<std::string>("stderr",
                                     lth_file::read(results_dir + "/stderr"));
        } else {
            std::string tmp { "Job '" + job_id + "' is in unknown state: " };
            results.set<std::string>("status", tmp + status_txt);
        }
    }

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace PXPAgent
