#include <pxp-agent/util/bolt_module.hpp>
#include <pxp-agent/util/utf8.hpp>

#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.bolt"
#include <leatherman/logging/logging.hpp>
#include <pxp-agent/configuration.hpp>

namespace PXPAgent {
namespace Util {

namespace fs = boost::filesystem;

namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;

#ifdef _WIN32
// The extension is required with lth_exec::execute when using a full path.
static const std::string EXECUTION_WRAPPER_EXECUTABLE { "execution_wrapper.exe" };
#else
static const std::string EXECUTION_WRAPPER_EXECUTABLE { "execution_wrapper" };
#endif


void BoltModule::processOutputAndUpdateMetadata(PXPAgent::ActionResponse &response) {
    if (response.output.std_out.empty()) {
        LOG_TRACE("Obtained no results on stdout for the {1}",
                  response.prettyRequestLabel());
    } else {
        LOG_TRACE("Results on stdout for the {1}: {2}",
                  response.prettyRequestLabel(), response.output.std_out);
    }

    if (response.output.exitcode != EXIT_SUCCESS) {
        std::stringstream ss_detail;

        if (!response.output.std_err.empty()) {
            ss_detail << "; stderr:\n" << response.output.std_err;
        } else if (response.output.std_out.empty() && response.output.exitcode == 127) {
            // When leatherman::execution attempts to execute a nonexistent command, the exit code
            // is 127 and both stdout and stderr are empty. We'll leave this result untouched,
            // but log a message explaining what happened:
            ss_detail << "; command not found";
        }

        LOG_TRACE("Execution failure (exit code {1}) for the {2}{3}",
                  response.output.exitcode, response.prettyRequestLabel(), ss_detail.str());
    } else if (!response.output.std_err.empty()) {
        LOG_TRACE("Output on stderr for the {1}:\n{2}",
                  response.prettyRequestLabel(), response.output.std_err);
    }

    std::string &output = response.output.std_out;

    if (Util::isValidUTF8(output)) {
        // Return all relevant results: exitcode, stdout, stderr.
        lth_jc::JsonContainer result;
        result.set("exitcode", response.output.exitcode);
        if (!output.empty()) {
            result.set("stdout", output);
        }
        if (!response.output.std_err.empty()) {
            result.set("stderr", response.output.std_err);
        }

        response.setValidResultsAndEnd(std::move(result));
    } else {
        LOG_DEBUG("Obtained invalid UTF-8 on stdout for the {1}; stdout:\n{2}",
                  response.prettyRequestLabel(), output);
        std::string execution_error {
                lth_loc::format("The task executed for the {1} returned invalid "
                                "UTF-8 on stdout - stderr:{2}",
                                response.prettyRequestLabel(),
                                (response.output.std_err.empty()
                                 ? lth_loc::translate(" (empty)")
                                 : "\n" + response.output.std_err)) };
        response.setBadResultsAndEnd(execution_error);
    }
}

leatherman::execution::result BoltModule::run_sync(const CommandObject &cmd) {
    return lth_exec::execute(
            cmd.executable,
            cmd.arguments,
            cmd.input,
            cmd.environment,
            cmd.pid_callback,
            0,  // timeout
            leatherman::util::option_set<lth_exec::execution_options> {
                    lth_exec::execution_options::thread_safe,
                    lth_exec::execution_options::merge_environment,
                    lth_exec::execution_options::inherit_locale
            });
}

leatherman::execution::result BoltModule::run(const CommandObject &cmd) {
    return lth_exec::execute(
            cmd.executable,
            cmd.arguments,
            cmd.input,
            cmd.environment,
            cmd.pid_callback,
            0,  // timeout
            leatherman::util::option_set<lth_exec::execution_options> {
                    lth_exec::execution_options::thread_safe,
                    lth_exec::execution_options::merge_environment,
                    lth_exec::execution_options::inherit_locale,
                    lth_exec::execution_options::create_detached_process
            });
}

void BoltModule::callBlockingAction(
        const ActionRequest& request,
        const Util::CommandObject &command,
        ActionResponse &response
) {
    auto exec = run_sync(command);
    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    processOutputAndUpdateMetadata(response);
}

void BoltModule::callNonBlockingAction(
        const ActionRequest& request,
        const Util::CommandObject &command,
        ActionResponse &response
) {
    // Guaranteed by Configuration
    assert(!request.resultsDir().empty());

    // Wrap the execution
    const fs::path &results_dir = request.resultsDir();
    lth_jc::JsonContainer wrapper_input;

    wrapper_input.set<std::string>("executable", command.executable);
    wrapper_input.set<std::vector<std::string>>("arguments", command.arguments);
    wrapper_input.set<std::string>("input", command.input);
    wrapper_input.set<std::string>("stdout", (results_dir / "stdout").string());
    wrapper_input.set<std::string>("stderr", (results_dir / "stderr").string());
    wrapper_input.set<std::string>("exitcode", (results_dir / "exitcode").string());

    CommandObject wrapped_command {
        (exec_prefix_ / EXECUTION_WRAPPER_EXECUTABLE).string(),
        {},
        command.environment,
        wrapper_input.toString(),
        [results_dir](size_t pid) {
            auto pid_file = (results_dir / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                           NIX_FILE_PERMS, std::ios::binary);
        }
    };

    auto exec = run(wrapped_command);

    // Stdout / stderr output should be on file, written by the execution wrapper:
    response.output = storage_->getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
}

ActionResponse BoltModule::invokeCommand(const ActionRequest& request, const CommandObject& cmd)
{
    ActionResponse response { ModuleType::Internal, request };

    if (request.type() == RequestType::Blocking) {
        callBlockingAction(request, cmd, response);
    } else {
        callNonBlockingAction(request, cmd, response);
    }

    return response;
}

}  // namespace Modules
}  // namespace PXPAgent
