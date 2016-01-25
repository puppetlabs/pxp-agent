#include <pxp-agent/modules/status.hpp>
#include <pxp-agent/util/process.hpp>
#include <pxp-agent/external_module.hpp>    // readNonBlockingOutcome
#include <pxp-agent/results_mutex.hpp>

#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.status"
#include <leatherman/logging/logging.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <string>
#include <stdexcept>
#include <stdint.h>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace HW = HorseWhisperer;
namespace pcp_util = PCPClient::Util;

static const std::string QUERY { "query" };
static const uint32_t METADATA_RACE_MS { 100 };

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
    results_validator_.registerSchema(output_schema);
}

class ActionMetadata {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    int exitcode;
    bool completed;

    ActionMetadata() {
    }

    ActionMetadata(const std::string& file_)
            : exitcode {},
              completed { false },
              file { file_ } {
        if (!fs::exists(file)) {
            throw Error { "file does not exist" };
        }

        std::string txt;
        if (!lth_file::read(file, txt)) {
            throw Error { "failed to read" };
        }

        try {
            lth_jc::JsonContainer entries { txt };

            if (entries.includes("completed")) {
                completed = entries.get<bool>("completed");
            } else {
                throw Error { "invalid content; missing 'completed' entry" };
            }

            if (completed) {
                if (entries.includes("exitcode")) {
                    exitcode = entries.get<int>("exitcode");
                } else {
                    throw Error { "invalid content; missing 'exitcode' entry" };
                }
            }
        } catch (const lth_jc::data_error& e) {
            LOG_DEBUG("Failed to parse JSON content of '%1%': %2%",
                      file, e.what());
            throw Error { std::string("invalid content format: ") + txt };
        }
    }

  private:
    std::string file;
};

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

    // Initialize the job status to 'unknown'
    results.set<std::string>("status", Status::UNKNOWN);

    if (!fs::exists(results_dir_path)) {
        LOG_DEBUG("Found no results for job %1%", t_id);
        return ActionOutcome { EXIT_SUCCESS, results };
    }

    LOG_DEBUG("Retrieving results for job %1% from %2%",
              t_id, results_dir_path.string());

    bool running_by_pid { false };
    bool not_running_by_pid { false };
    auto pid_file = (results_dir_path / "pid").string();
    std::string pid_txt {};
    int pid {};

    // Read the PID file before the metadata, to avoid a race in the
    // non-blocking task thread, due to the elapsed time between the
    // end of the job and the metadata write
    if (!fs::exists(pid_file)) {
        LOG_DEBUG("PID file '%1%' does not exist", pid_file);
    } else if (!lth_file::read(pid_file, pid_txt)) {
        LOG_ERROR("Failed to read PID file '%1%'", pid_file);
    } else if (pid_txt.empty()) {
        LOG_ERROR("PID file '%1%' is empty", pid_file);
    } else {
        try {
            pid = std::stoi(pid_txt);
            // NOTE(ale): processExists() does not throw
            if (Util::processExists(pid)) {
                // NOTE(ale): will set status to 'running' iff
                // metadata is valid and metadata.completed == false
                running_by_pid = true;
            } else {
                not_running_by_pid = true;
                bool cached { false };
                {
                    ResultsMutex::LockGuard a_l {
                        ResultsMutex::Instance().access_mtx };  // RAII
                    cached = ResultsMutex::Instance().exists(t_id);
                }
                if (cached) {
                    // The process doe not exist anymore, but its
                    // mutex is still cached - wait a bit before
                    // reading the metadata to allow the non-blocking
                    // to lock its mutex and update the metadata
                    pcp_util::this_thread::sleep_for(
                        pcp_util::chrono::milliseconds(METADATA_RACE_MS));
                }
            }
        } catch (const std::invalid_argument& e) {
            LOG_ERROR("Invalid value '%1%' stored in PID file '%2%'",
                      pid_txt, pid_file);
        }
    }

    ActionMetadata metadata {};
    auto metadata_file = (results_dir_path / "metadata").string();

    try {
        // Acquire the result mutex lock, if the transaction is cached
        ResultsMutex::Mutex_Ptr mtx_ptr;
        {
            ResultsMutex::LockGuard a_l {
                ResultsMutex::Instance().access_mtx };  // RAII
            if (ResultsMutex::Instance().exists(t_id)) {
                mtx_ptr = ResultsMutex::Instance().get(t_id);
            }
        }
        if (mtx_ptr != nullptr) {
            LOG_TRACE("Non-blocking transaction %1% mutex is cached; "
                      "locking it", t_id);
            ResultsMutex::LockGuard r_l { *mtx_ptr };  // RAII
            metadata = ActionMetadata(metadata_file);
        } else {
            LOG_TRACE("Non-blocking transaction %1% mutex is not cached", t_id);
            metadata = ActionMetadata(metadata_file);
        }
    } catch (const ActionMetadata::Error& e) {
        // TODO(ale): consider reporting back the read failure
        // The file may not exist, may not be readable, or contain
        // invalid JSON - return "unknown"
        LOG_ERROR("Cannot retrieve metadata from %1%: %2%", metadata_file, e.what());
        return ActionOutcome { EXIT_SUCCESS, results };
    } catch (const std::exception& e) {
        // Unexpected as we locked access_mtx before getting the mutex
        LOG_ERROR("Unexpected failure while retrieving metadata from %1%: %2%",
                  metadata_file, e.what());
        return ActionOutcome { EXIT_SUCCESS, results };
    }

    // At this point, we know that the metadata file is valid
    if (metadata.completed) {
        // We consider the job completed, regardless of the PID
        results.set<std::string>(
            "status",
            (metadata.exitcode == EXIT_SUCCESS ? Status::SUCCESS : Status::FAILURE));
    } else if (running_by_pid) {
        results.set<std::string>("status", Status::RUNNING);
    }

    if (metadata.completed || not_running_by_pid) {
        assert(!running_by_pid);
        // The process is either completed (status may be 'failure' or
        // 'success', depending on the exit code) or not running
        // (after checking the pid - status is 'unknown'); we can send
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

        results.set<int>("exitcode", metadata.exitcode);
        results.set<std::string>("stdout", o);
        results.set<std::string>("stderr", e);
    }

    return ActionOutcome { EXIT_SUCCESS, results };
}

}  // namespace Modules
}  // namespace PXPAgent
