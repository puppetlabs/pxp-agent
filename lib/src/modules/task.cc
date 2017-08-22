#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <rapidjson/rapidjson.h>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;

namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;

static const std::string TASK_RUN_ACTION { "run" };

static const lth_jc::JsonContainer TASK_RUN_ACTION_INPUT_SCHEMA { R"(
{
  "type": "object",
  "properties": {
    "task": {
      "type": "string"
    },
    "files": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "filename": {
            "type": "string"
          },
          "uri": {
            "type": "object",
            "properties": {
              "path": {
                "type": "string"
              },
              "params": {
                 "type": "object"
              }
            },
            "required": ["path", "params"]
          },
          "sha256": {
            "type": "string"
          }
        },
        "required": ["filename", "uri", "sha256"]
      },
      "minItems": 1
    },
    "input": {
      "type": "object"
    },
    "inputFormat": {
      "type": "string"
    }
  },
  "required": ["task", "files", "input"]
}
)" };

#ifdef _WIN32
// The extension is required with lth_exec::execute when using a full path.
static const std::string TASK_WRAPPER_EXECUTABLE { "task_wrapper.exe" };
#else
static const std::string TASK_WRAPPER_EXECUTABLE { "task_wrapper" };
#endif
static const int TASK_WRAPPER_FILE_ERROR_EC { 5 };

// Hard-code interpreters on Windows. On non-Windows, we still rely on permissions and #!
static const std::map<std::string, std::function<TaskCommand(std::string)>> BUILTIN_TASK_INTERPRETERS {
#ifdef _WIN32
    {".rb",  [](std::string filename) { return TaskCommand {
        "ruby", { filename }
    }; }},
    {".pp",  [](std::string filename) { return TaskCommand {
        "puppet", { "apply", filename }
    }; }},
    {".ps1", [](std::string filename) { return TaskCommand {
        "powershell", { "-NoProfile", "-NonInteractive", "-NoLogo", "-ExecutionPolicy", "Bypass", "-File", filename }
    }; }}
#endif
};

Task::Task(const fs::path& exec_prefix,
           const std::string& task_cache_dir,
           const std::string& spool_dir) :
    storage_ { spool_dir },
    task_cache_dir_ { task_cache_dir },
    wrapper_executable_ { (exec_prefix / TASK_WRAPPER_EXECUTABLE).string() }
{
    module_name = "task";
    actions.push_back(TASK_RUN_ACTION);

    PCPClient::Schema input_schema { TASK_RUN_ACTION, TASK_RUN_ACTION_INPUT_SCHEMA };
    PCPClient::Schema output_schema { TASK_RUN_ACTION };

    input_validator_.registerSchema(input_schema);
    results_validator_.registerSchema(output_schema);
}

static void addParametersToEnvironment(const lth_jc::JsonContainer &input, std::map<std::string, std::string> &environment)
{
    for (auto& k : input.keys()) {
        auto val = input.type(k) == lth_jc::String ? input.get<std::string>(k) : input.toString(k);
        environment.emplace("PT_"+k, move(val));
    }
}

static TaskCommand getTaskCommand(const fs::path &task_executable)
{
    auto builtin = BUILTIN_TASK_INTERPRETERS.find(task_executable.extension().string());
    if (builtin != BUILTIN_TASK_INTERPRETERS.end()) {
        return builtin->second(task_executable.string());
    }

    return TaskCommand { task_executable.string(), { } };
}

// Verify (this includes checking the SHA256 checksums) that *all* task files are present
// in the task cache downloading them if necessary.
// Return the full path of the cached version of the first file from the list (which
// is assumed to be the task executable).
static fs::path getCachedTaskFile(const std::string& task_cache_dir,
                                  const std::vector<lth_jc::JsonContainer> &files) {
    if (files.empty()) {
        throw Module::ProcessingError {
            lth_loc::format("at least one file must be specified for a task") };
    }
    auto file = files[0];
    LOG_DEBUG("Verifying task file based on {1}", file.toString());

    // TODO task file verification and downloading
    // This is a temporary hack to make existing workflows continue to work.
    return { task_cache_dir + file.get<std::string>({"uri", "path"}) };
}

void Task::callBlockingAction(
    const ActionRequest& request,
    const TaskCommand &command,
    const std::map<std::string, std::string> &environment,
    const std::string &input,
    ActionResponse &response
) {
    auto exec = lth_exec::execute(
        command.executable,
        command.arguments,
        input,
        environment,
        0,  // timeout
        leatherman::util::option_set<lth_exec::execution_options> {
            lth_exec::execution_options::thread_safe,
            lth_exec::execution_options::merge_environment,
            lth_exec::execution_options::inherit_locale });  // options

    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    processOutputAndUpdateMetadata(response);
}

void Task::callNonBlockingAction(
    const ActionRequest& request,
    const TaskCommand &command,
    const std::map<std::string, std::string> &environment,
    const std::string &input,
    ActionResponse &response
) {
    const fs::path &results_dir = request.resultsDir();
    lth_jc::JsonContainer wrapper_input;

    wrapper_input.set<std::string>("executable", command.executable);
    wrapper_input.set<std::vector<std::string>>("arguments", command.arguments);
    wrapper_input.set<std::string>("input", input);
    wrapper_input.set<std::string>("stdout", (results_dir / "stdout").string());
    wrapper_input.set<std::string>("stderr", (results_dir / "stderr").string());
    wrapper_input.set<std::string>("exitcode", (results_dir / "exitcode").string());

    auto exec = lth_exec::execute(
        wrapper_executable_,
        {},
        wrapper_input.toString(),
        environment,
        [results_dir](size_t pid) {
            auto pid_file = (results_dir / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                           NIX_FILE_PERMS, std::ios::binary);
        },  // pid callback
        0,  // timeout
        leatherman::util::option_set<lth_exec::execution_options> {
            lth_exec::execution_options::thread_safe,
            lth_exec::execution_options::create_detached_process,
            lth_exec::execution_options::merge_environment,
            lth_exec::execution_options::inherit_locale });  // options

    if (exec.exit_code == TASK_WRAPPER_FILE_ERROR_EC) {
        // This is unexpected. The output of the task will not be
        // available for future transaction status requests; we cannot
        // provide a reliable ActionResponse.
        std::string empty_label { lth_loc::translate("(empty)") };
        LOG_WARNING("The task wrapper process failed to write output on file for the {1}; "
                    "stdout: {2}; stderr: {3}",
                    request.prettyLabel(),
                    (exec.output.empty() ? empty_label : exec.output),
                    (exec.error.empty() ? empty_label : exec.error));
        throw Module::ProcessingError {
            lth_loc::translate("failed to write output on file") };
    }

    // Stdout / stderr output should be on file; read it
    response.output = storage_.getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
}

ActionResponse Task::callAction(const ActionRequest& request)
{
    auto task_execution_params = request.params();
    auto task_input_format = task_execution_params.includes("inputFormat") ?
        task_execution_params.get<std::string>("inputFormat") : std::string{""};
    std::map<std::string, std::string> task_environment;
    std::string task_input;

    bool has_input = false;
    if (task_input_format.empty() || task_input_format == "stdin:json") {
        task_input = task_execution_params.get<lth_jc::JsonContainer>("input").toString();
        has_input = true;
    }

    if (task_input_format.empty() || task_input_format == "environment") {
        addParametersToEnvironment(task_execution_params.get<lth_jc::JsonContainer>("input"), task_environment);
        has_input = true;
    }

    if (!has_input) {
        throw Module::ProcessingError {
            lth_loc::format("unsupported task input format: {1}", task_input_format) };
    }

    auto task_command = getTaskCommand(
        getCachedTaskFile(task_cache_dir_,
                          task_execution_params.get<std::vector<lth_jc::JsonContainer>>("files")));
    ActionResponse response { ModuleType::Internal, request };

    if (request.type() == RequestType::Blocking) {
        callBlockingAction(request, task_command, task_environment, task_input, response);
    } else {
        // Guaranteed by Configuration
        assert(!request.resultsDir().empty());
        callNonBlockingAction(request, task_command, task_environment, task_input, response);
    }

    return response;
}

static bool isValidUTF8(std::string &s)
{
    rapidjson::StringStream source(s.data());
    rapidjson::InsituStringStream target(&s[0]);

    target.PutBegin();
    while (source.Tell() < s.size()) {
        if (!rapidjson::UTF8<char>::Validate(source, target)) {
            return false;
        }
    }
    return true;
}

void Task::processOutputAndUpdateMetadata(ActionResponse& response)
{
    if (response.output.std_out.empty()) {
        LOG_TRACE("Obtained no results on stdout for the {1}",
                  response.prettyRequestLabel());
    } else {
        LOG_TRACE("Results on stdout for the {1}: {2}",
                  response.prettyRequestLabel(), response.output.std_out);
    }

    if (response.output.exitcode != EXIT_SUCCESS) {
        LOG_TRACE("Execution failure (exit code {1}) for the {2}{3}",
                  response.output.exitcode, response.prettyRequestLabel(),
                  (response.output.std_err.empty()
                        ? ""
                        : "; stderr:\n" + response.output.std_err));
    } else if (!response.output.std_err.empty()) {
        LOG_TRACE("Output on stderr for the {1}:\n{2}",
                  response.prettyRequestLabel(), response.output.std_err);
    }

    std::string &output = response.output.std_out;

    if (isValidUTF8(output)) {
        lth_jc::JsonContainer stdout_result;
        stdout_result.set("output", output);

        response.setValidResultsAndEnd(std::move(stdout_result));
    } else {
        LOG_DEBUG("Obtained invalid UTF-8 on stdout for the {1}; stdout:\n{3}",
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

}  // namespace Modules
}  // namespace PXPAgent
