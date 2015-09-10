#include <pxp-agent/modules/status.hpp>
#include <pxp-agent/configuration.hpp>
#include <boost/filesystem.hpp>

#include <leatherman/file_util/file.hpp>

#include <fstream>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.status"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace lth_file = leatherman::file_util;

static const std::string QUERY { "query" };

Status::Status() {
    module_name = "status";
    actions.push_back(QUERY);
    PCPClient::Schema input_schema { QUERY };
    input_schema.addConstraint("transaction_id", PCPClient::TypeConstraint::String,
                               true);

    PCPClient::Schema output_schema { QUERY };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Status::callAction(const ActionRequest& request) {
    lth_jc::JsonContainer results {};
    auto t_id = request.params().get<std::string>("transaction_id");
    auto results_dir = Configuration::Instance().get<std::string>("spool-dir") + t_id;

    if (!boost::filesystem::exists(results_dir)) {
        LOG_ERROR("Found no results for job %1%", t_id);
        results.set<std::string>("status", "Unknown");
    } else {
        LOG_DEBUG("Retrieving results for job %1% from %2%", t_id, results_dir);
        lth_jc::JsonContainer status_data { lth_file::read(results_dir + "/status") };

        auto status_txt = status_data.get<std::string>("status");
        auto exitcode = status_data.get<int>("exitcode");;

        if (status_txt == "running") {
            results.set<std::string>("status", "running");
        } else if (status_txt == "completed") {
            std::string status { (exitcode == EXIT_SUCCESS ? "success" : "failure") };
            auto err = lth_file::read(results_dir + "/stderr");
            auto out = lth_file::read(results_dir + "/stdout");

            results.set<std::string>("status", status);
            results.set<int>("exitcode", exitcode);
            results.set<std::string>("stdout", out);
            results.set<std::string>("stderr", err);
        } else {
            results.set<std::string>("status", "unknown");
        }
    }

    return ActionOutcome { EXIT_SUCCESS, results };
}

}  // namespace Modules
}  // namespace PXPAgent
