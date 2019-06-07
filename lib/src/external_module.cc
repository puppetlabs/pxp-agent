#include <pxp-agent/external_module.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/action_output.hpp>
#include <pxp-agent/configuration.hpp>

#include <leatherman/execution/execution.hpp>

#include <leatherman/file_util/file.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.external_module"
#include <leatherman/logging/logging.hpp>

#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <atomic>
#include <memory>   // std::shared_ptr
#include <utility>  // std::move

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

namespace PXPAgent {

static const std::string METADATA_SCHEMA_NAME { "external_module_metadata" };
static const std::string ACTION_SCHEMA_NAME { "action_metadata" };

static const std::string METADATA_CONFIGURATION_ENTRY { "configuration" };
static const std::string METADATA_ACTIONS_ENTRY { "actions" };

static const int EXTERNAL_MODULE_FILE_ERROR_EC { 5 };

namespace fs = boost::filesystem;
namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;
namespace pcp_util = PCPClient::Util;

//
// Free functions
//

// Provides the module metadata validator
PCPClient::Validator getMetadataValidator()
{
    // Metadata schema
    PCPClient::Schema metadata_schema { METADATA_SCHEMA_NAME,
                                          PCPClient::ContentType::Json };
    using T_C = PCPClient::TypeConstraint;
    metadata_schema.addConstraint("description", T_C::String, true);
    metadata_schema.addConstraint(METADATA_CONFIGURATION_ENTRY, T_C::Object, false);
    metadata_schema.addConstraint(METADATA_ACTIONS_ENTRY, T_C::Array, true);

    // 'actions' is an array of actions; define the action sub_schema
    PCPClient::Schema action_schema { ACTION_SCHEMA_NAME,
                                      PCPClient::ContentType::Json };
    action_schema.addConstraint("description", T_C::String, false);
    action_schema.addConstraint("name", T_C::String, true);
    action_schema.addConstraint("input", T_C::Object, true);
    action_schema.addConstraint("results", T_C::Object, true);

    metadata_schema.addConstraint(METADATA_ACTIONS_ENTRY, action_schema, false);

    PCPClient::Validator validator {};
    validator.registerSchema(metadata_schema);
    return validator;
}

//
// Public interface
//

ExternalModule::ExternalModule(const std::string& path,
                               const lth_jc::JsonContainer& config,
                               std::shared_ptr<ResultsStorage> storage)
        : path_ { path },
          config_ { config },
          storage_ { std::move(storage) }
{
    fs::path module_path { path };
    module_name = module_path.stem().string();
    auto metadata = getModuleMetadata();

    try {
        if (metadata.includes(METADATA_CONFIGURATION_ENTRY)) {
            registerConfiguration(
                metadata.get<lth_jc::JsonContainer>(METADATA_CONFIGURATION_ENTRY));
        } else {
            LOG_DEBUG("Found no configuration schema for module '{1}'", module_name);
        }

        registerActions(metadata);
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata of module {1}: {2}",
                  module_name, e.what());
        throw Module::LoadingError {
            lth_loc::format("invalid metadata of module {1}", module_name) };
    }
}

ExternalModule::ExternalModule(const std::string& path,
                               std::shared_ptr<ResultsStorage> storage)
        : path_ { path },
          config_ { "{}" },
          storage_ { std::move(storage) }
{
    fs::path module_path { path };
    module_name = module_path.stem().string();
    auto metadata = getModuleMetadata();

    try {
       registerActions(metadata);
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata of module {1}: {2}",
                  module_name, e.what());
        throw Module::LoadingError {
            lth_loc::format("invalid metadata of module {1}", module_name) };
    }
}

void ExternalModule::validateConfiguration()
{
    if (config_validator_.includesSchema(module_name)) {
        config_validator_.validate(config_, module_name);
    } else {
        LOG_DEBUG("The '{1}' configuration will not be validated; no JSON "
                  "schema is available", module_name);
    }
}

//
// Static class members
//

// cppcheck-suppress constStatement
const int ExternalModule::OUTPUT_DELAY_MS { 100 };

void ExternalModule::processOutputAndUpdateMetadata(ActionResponse& response)
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

    try {
        // Ensure output format is valid JSON by instantiating
        // JsonContainer (NB: its ctor does not accept empty strings)
        lth_jc::JsonContainer results {
            (response.output.std_out.empty() ? "null" : response.output.std_out) };
        response.setValidResultsAndEnd(std::move(results));
    } catch (lth_jc::data_parse_error& e) {
        LOG_DEBUG("Obtained invalid JSON on stdout for the {1}; (validation "
                  "error: {2}); stdout:\n{3}",
                  response.prettyRequestLabel(), e.what(), response.output.std_out);
        std::string execution_error {
            lth_loc::format("The task executed for the {1} returned invalid "
                            "JSON on stdout - stderr:{2}",
                            response.prettyRequestLabel(),
                            (response.output.std_err.empty()
                                ? lth_loc::translate(" (empty)")
                                : "\n" + response.output.std_err)) };
        response.setBadResultsAndEnd(execution_error);
    }
}

//
// Private interface
//

// Metadata validator (static member)
const PCPClient::Validator ExternalModule::metadata_validator_ {
        getMetadataValidator() };

// Retrieve and validate the module's metadata
const lth_jc::JsonContainer ExternalModule::getModuleMetadata()
{
    auto exec = lth_exec::execute(
#ifdef _WIN32
        "cmd.exe", { "/c", path_, "metadata" },
#else
        path_, { "metadata" },
#endif
        0,  // timeout
        { lth_exec::execution_options::thread_safe,
          lth_exec::execution_options::merge_environment,
          lth_exec::execution_options::inherit_locale });  // options

    if (!exec.error.empty()) {
        LOG_ERROR("Failed to load the external module metadata from {1}: {2}",
                  path_, exec.error);
        throw Module::LoadingError {
            lth_loc::translate("failed to load external module metadata") };
    }

    lth_jc::JsonContainer metadata;

    try {
        metadata = lth_jc::JsonContainer { exec.output };
        LOG_DEBUG("External module {1}: metadata is valid JSON", module_name);
    } catch (lth_jc::data_error& e) {
        throw Module::LoadingError {
            lth_loc::format("metadata is not in a valid JSON format: {1}", e.what()) };
    }

    try {
        metadata_validator_.validate(metadata, METADATA_SCHEMA_NAME);
        LOG_DEBUG("External module {1}: metadata validation OK", module_name);
    } catch (PCPClient::validation_error& e) {
        throw Module::LoadingError {
            lth_loc::format("metadata validation failure: {1}", e.what()) };
    }

    return metadata;
}

void ExternalModule::registerConfiguration(const lth_jc::JsonContainer& config_metadata)
{
    try {
        PCPClient::Schema configuration_schema { module_name, config_metadata };
        LOG_DEBUG("Registering module configuration schema for '{1}'", module_name);
        config_validator_.registerSchema(configuration_schema);
    } catch (PCPClient::schema_error& e) {
        LOG_ERROR("Failed to parse the configuration schema of module '{1}': {2}",
                  module_name, e.what());
        throw Module::LoadingError {
            lth_loc::format("invalid configuration schema of module {1}", module_name) };
    }
}

void ExternalModule::registerActions(const lth_jc::JsonContainer& metadata)
{
    for (auto& action
            : metadata.get<std::vector<lth_jc::JsonContainer>>(METADATA_ACTIONS_ENTRY))
        registerAction(action);
}

// Register the specified action after ensuring that the input and
// output schemas are valid JSON (i.e. we can instantiate Schema).
void ExternalModule::registerAction(const lth_jc::JsonContainer& action)
{
    // NOTE(ale): name, input, and output are required action entries
    auto action_name = action.get<std::string>("name");
    LOG_DEBUG("Validating action '{1} {2}'", module_name, action_name);

    try {
        auto input_schema_json = action.get<lth_jc::JsonContainer>("input");
        PCPClient::Schema input_schema { action_name, input_schema_json };

        auto results_schema_json = action.get<lth_jc::JsonContainer>("results");
        PCPClient::Schema results_schema { action_name, results_schema_json };

        // Metadata schemas are valid JSON; store metadata
        LOG_DEBUG("Action '{1} {2}' has been validated", module_name, action_name);
        actions.push_back(action_name);
        input_validator_.registerSchema(input_schema);
        results_validator_.registerSchema(results_schema);
    } catch (PCPClient::schema_error& e) {
        LOG_ERROR("Failed to parse metadata schemas of action '{1} {2}': {3}",
                  module_name, action_name, e.what());
        throw Module::LoadingError {
            lth_loc::format("invalid schemas of '{1} {2}'", module_name, action_name) };
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata schemas of action '{1} {2}': {3}",
                  module_name, action_name, e.what());
        throw Module::LoadingError {
            lth_loc::format("invalid metadata of '{1} {2}'", module_name, action_name) };
    }
}

std::string ExternalModule::getActionArguments(const ActionRequest& request)
{
    lth_jc::JsonContainer action_args {};
    action_args.set<lth_jc::JsonContainer>("input", request.params());

    if (!config_.empty())
        action_args.set<lth_jc::JsonContainer>("configuration", config_);

    if (request.type() == RequestType::NonBlocking) {
        fs::path r_d_p { request.resultsDir() };
        lth_jc::JsonContainer output_files {};
        output_files.set<std::string>("stdout", (r_d_p / "stdout").string());
        output_files.set<std::string>("stderr", (r_d_p / "stderr").string());
        output_files.set<std::string>("exitcode", (r_d_p / "exitcode").string());
        action_args.set<lth_jc::JsonContainer>("output_files", output_files);
    }

    return action_args.toString();
}

ActionResponse ExternalModule::callBlockingAction(const ActionRequest& request)
{
    ActionResponse response { ModuleType::External, request };
    auto action_name = request.action();
    auto action_args = getActionArguments(request);

    LOG_INFO("Executing the {1}", request.prettyLabel());
    LOG_TRACE("Input for the {1}: {2}", request.prettyLabel(), action_args);

    auto exec = lth_exec::execute(
#ifdef _WIN32
        "cmd.exe", { "/c", path_, action_name },
#else
        path_, { action_name },
#endif
        action_args,                           // args
        std::map<std::string, std::string>(),  // environment
        0,                                     // timeout
        { lth_exec::execution_options::thread_safe,
          lth_exec::execution_options::merge_environment,
          lth_exec::execution_options::inherit_locale });  // options

    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    processOutputAndUpdateMetadata(response);
    return response;
}

ActionResponse ExternalModule::callNonBlockingAction(const ActionRequest& request)
{
    ActionResponse response { ModuleType::External, request };
    auto action_name = request.action();
    auto input_txt = getActionArguments(request);
    fs::path results_dir_path { request.resultsDir() };

    LOG_INFO("Starting a task for the {1}; stdout and stderr will be stored in {2}",
             request.prettyLabel(), request.resultsDir());
    LOG_TRACE("Input for the {1}: {2}", request.prettyLabel(), input_txt);

    // NOTE(ale,mruzicka): to avoid terminating the entire process
    // tree when the pxp-agent service stops, we use the
    // `create_detached_process` execution option which ensures
    // the child process is executed in a new process contract
    // on Solaris and a new process group on Windows

    auto exec = lth_exec::execute(
#ifdef _WIN32
        "cmd.exe", { "/c", path_, action_name },
#else
        path_, { action_name },
#endif
        input_txt,  // input arguments, passed via stdin
        std::map<std::string, std::string>(),  // environment
        [results_dir_path](size_t pid) {
            auto pid_file = (results_dir_path / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                           NIX_FILE_PERMS, std::ios::binary);
        },          // pid callback
        0,          // timeout
        { lth_exec::execution_options::thread_safe,
          lth_exec::execution_options::create_detached_process,
          lth_exec::execution_options::merge_environment,
          lth_exec::execution_options::inherit_locale });  // options

    LOG_INFO("The execution of the {1} has completed", request.prettyLabel());

    if (exec.exit_code == EXTERNAL_MODULE_FILE_ERROR_EC) {
        // This is unexpected. The output of the task will not be
        // available for future transaction status requests; we cannot
        // provide a reliable ActionResponse.
        std::string empty_label { lth_loc::translate("(empty)") };
        LOG_WARNING("The execution process failed to write output on file for the {1}; "
                    "stdout: {2}; stderr: {3}",
                    request.prettyLabel(),
                    (exec.output.empty() ? empty_label : exec.output),
                    (exec.error.empty() ? empty_label : exec.error));
        throw Module::ProcessingError {
            lth_loc::translate("failed to write output on file") };
    }

    // Wait a bit to relax the requirement for the exitcode file being
    // written before the output ones, when the process completes
    LOG_TRACE("Waiting {1} ms before retrieving the output of {2}",
              OUTPUT_DELAY_MS, request.prettyLabel());
    pcp_util::this_thread::sleep_for(
        pcp_util::chrono::milliseconds(OUTPUT_DELAY_MS));

    // Stdout / stderr output should be on file; read it
    response.output = storage_->getOutput(request.transactionId(), exec.exit_code);
    processOutputAndUpdateMetadata(response);
    return response;
}

ActionResponse ExternalModule::callAction(const ActionRequest& request)
{
    if (request.type() == RequestType::Blocking) {
        return callBlockingAction(request);
    } else {
        // Guranteed by Configuration
        assert(!request.resultsDir().empty());
        return callNonBlockingAction(request);
    }
}

}  // namespace PXPAgent
