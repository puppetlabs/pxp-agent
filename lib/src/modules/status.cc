#include <pxp-agent/modules/status.hpp>
#include <pxp-agent/util/process.hpp>
#include <pxp-agent/external_module.hpp>    // readNonBlockingOutcome

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.status"
#include <leatherman/logging/logging.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <string>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace HW = HorseWhisperer;

static const std::string QUERY { "query" };

const std::string Status::UNKNOWN { "unknown" };
const std::string Status::SUCCESS { "success" };
const std::string Status::FAILURE { "failure" };
const std::string Status::RUNNING { "running" };

Status::Status() {
    module_name = "status";
    actions.push_back(QUERY);
    PCPClient::Schema input_schema { QUERY };
    input_schema.addConstraint("transaction_id",
                               PCPClient::TypeConstraint::String,
                               true);
    PCPClient::Schema output_schema { QUERY };
    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

//
//                          STATUS TABLE
//
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |         |          |          |           |                     |
// |  valid  |completed?| PID file |PID process|      response:      |
// |metadata?|          |  exists? |  exists?  |       status        |
// |         |          |          |           |         &           |
// |         |          |          |           |   included files    |
// |         |          |          |           |                     |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   no    |     -    |     -    |     -     |       unknown       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |    no    |     -     |       unknown       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |    yes   |    yes    |       running       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |    yes   |    no     |       unknown       |
// |         |          |          |           | + stdout & stderr   |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    yes   |     -    |     -     |  success / failure  |
// |         |          |          |           | + stdout & stderr   |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
//

ActionOutcome Status::callAction(const ActionRequest& request) {
    lth_jc::JsonContainer results {};
    auto t_id = request.params().get<std::string>("transaction_id");
    fs::path spool_path { HW::GetFlag<std::string>("spool-dir") };
    auto results_dir_path = spool_path / t_id;
    results.set<std::string>("transaction_id", t_id);
    results.set<std::string>("status", Status::UNKNOWN);

    if (!fs::exists(results_dir_path)) {
        LOG_DEBUG("Found no results for job %1%", t_id);
        return ActionOutcome { EXIT_SUCCESS, results };
    }

    LOG_DEBUG("Retrieving results for job %1% from %2%",
              t_id, results_dir_path.string());

    std::string metadata_txt;
    lth_jc::JsonContainer action_metadata {};
    int exitcode {};
    auto metadata_file = (results_dir_path / "metadata").string();
    bool completed_by_metadata { false };
    bool valid_metadata { false };

    if (!fs::exists(metadata_file)) {
        LOG_ERROR("Metadata file '%1%' does not exist", metadata_file);
    } else if (!lth_file::read(metadata_file, metadata_txt)) {
        LOG_ERROR("Failed to read '%1%'", metadata_file);
    } else {
        try {
            action_metadata = lth_jc::JsonContainer(metadata_txt);

            if (action_metadata.includes("exitcode")
                    && action_metadata.includes("completed")) {
                valid_metadata = true;
                completed_by_metadata = action_metadata.get<bool>("completed");
                if (completed_by_metadata) {
                    exitcode = action_metadata.get<int>("exitcode");
                }
            } else {
                LOG_ERROR("Invalid metadata file '%1%': missing entries: %2%",
                          metadata_file, metadata_txt);
            }
        } catch (const lth_jc::data_error& e) {
            LOG_ERROR("Metadata file '%1%' has invalid format: %2%",
                      metadata_file, e.what());
        }
    }

    if (!valid_metadata) {
        // The file may not exist, may not be readable, or contain
        // invalid JSON - return "unknown"
        return ActionOutcome { EXIT_SUCCESS, results };
    }

    bool not_running_by_pid { false };

    if (completed_by_metadata) {
        results.set<std::string>(
            "status",
            (exitcode == EXIT_SUCCESS ? Status::SUCCESS : Status::FAILURE));
    } else {
        // The metadata does not report the task as completed, but it
        // may be due to a previous pxp-agent crash; if the PID file
        // is available, check if the process is running
        auto pid_file = (results_dir_path / "pid").string();
        std::string pid_txt;
        int pid {};

        if (!fs::exists(pid_file)) {
            LOG_ERROR("PID file '%1%' does not exist", pid_file);
        } else if (!lth_file::read(pid_file, pid_txt)) {
            LOG_ERROR("Failed to read PID file '%1%'", pid_file);
        } else if (pid_txt.empty()) {
            LOG_ERROR("PID file '%1%' is empty", pid_file);
        } else {
            try {
                pid = std::stoi(pid_txt);
                // NOTE(ale): processExists() does not throw
                if (Util::processExists(pid)) {
                    results.set<std::string>("status", Status::RUNNING);
                } else {
                    // We know that the process is not running, but
                    // its status is 'unknown'
                    not_running_by_pid = true;
                }
            } catch (const std::invalid_argument& e) {
                // We didn't manage to get the PID and the metadata
                // file does not report the action as completed; we
                // cannot determine the state, so leave it 'unknown'
                LOG_ERROR("Invalid value '%1%' stored in PID file '%2%'",
                          pid_txt, pid_file);
            }
        }
    }

    if (completed_by_metadata || not_running_by_pid) {
        // The process is either completed (state may be 'failure' or
        // 'success', depending on the exit code) or not running
        // (after checking the pid - state is 'unknown'); we can send
        // back the contents of stdout / stderr files
        std::string o;
        std::string e;
        auto o_f = (results_dir_path / "stdout").string();
        auto e_f = (results_dir_path / "stderr").string();

        try {
            ExternalModule::readNonBlockingOutcome(request, o_f, e_f, o, e);
        } catch (Module::ProcessingError) {
            // Failed to read o_f; continue, to send back e_f

            // TODO(ale): consider reporting back the reading failure
            // of the stdout file; perhaps by adding the "exec_error"
            // to the "properties" object together with the metadata
        }

        results.set<int>("exitcode", exitcode);
        results.set<std::string>("stdout", o);
        results.set<std::string>("stderr", e);
    }

    return ActionOutcome { EXIT_SUCCESS, results };
}

}  // namespace Modules
}  // namespace PXPAgent
