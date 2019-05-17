#include <rapidjson/rapidjson.h>
#if RAPIDJSON_MAJOR_VERSION > 1 || RAPIDJSON_MAJOR_VERSION == 1 && RAPIDJSON_MINOR_VERSION >= 1
// Header for StringStream was added in rapidjson 1.1 in a backwards incompatible way.
#include <rapidjson/stream.h>
#endif

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.command"
#include <leatherman/logging/logging.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/execution/execution.hpp>
#include <pxp-agent/modules/command.hpp>
#include <pxp-agent/module_type.hpp>

#include <utility>  // for std::move

namespace PXPAgent {
namespace Modules {

namespace lth_exec = leatherman::execution;
namespace lth_jc = leatherman::json_container;
namespace lth_loc  = leatherman::locale;

static const std::string COMMAND_RUN_ACTION { "run" };

// TODO: copied directly from Modules::Task
// Move to some common file
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

// TODO: copied directly from Modules::Task
// Reuse this from a base class instead, probably
void Command::processOutputAndUpdateMetadata(ActionResponse& response)
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

Command::Command()
{
    module_name = "command";
    actions.push_back(COMMAND_RUN_ACTION);

    PCPClient::Schema input_schema { COMMAND_RUN_ACTION };
    input_schema.addConstraint("command", PCPClient::TypeConstraint::String, true);
    // TODO: implement run_as
    input_schema.addConstraint("run_as", PCPClient::TypeConstraint::String, false);
    input_validator_.registerSchema(input_schema);

    PCPClient::Schema output_schema { COMMAND_RUN_ACTION };
    results_validator_.registerSchema(output_schema);
}

leatherman::execution::result Command::run(const ActionRequest& request)
{
    auto params = request.params();

    assert(params.includes("command") &&
           params.type("command") == lth_jc::DataType::String);

    auto exec = lth_exec::execute(
            params.get<std::string>("command"),
            0, // timeout
            leatherman::util::option_set<lth_exec::execution_options>{
                    lth_exec::execution_options::thread_safe,
                    lth_exec::execution_options::merge_environment,
                    lth_exec::execution_options::inherit_locale,
            });
    return exec;
}

ActionResponse Command::callAction(const ActionRequest& request)
{
    auto exec = run(request);
    ActionResponse response { ModuleType::Internal, request };
    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    processOutputAndUpdateMetadata(response);
    return response;
}

} // namespace Modules
} // namespace PXPAgent
