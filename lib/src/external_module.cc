#include <pxp-agent/external_module.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/action_output.hpp>

#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.external_module"
#include <leatherman/logging/logging.hpp>

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
namespace lth_jc = leatherman::json_container;

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
                               const std::string& spool_dir)
        : path_ { path },
          config_ { config },
          storage_ { spool_dir }
{
    fs::path module_path { path };
    module_name = module_path.stem().string();
    auto metadata = getModuleMetadata();

    try {
        if (metadata.includes(METADATA_CONFIGURATION_ENTRY)) {
            registerConfiguration(
                metadata.get<lth_jc::JsonContainer>(METADATA_CONFIGURATION_ENTRY));
        } else {
            LOG_DEBUG("Found no configuration schema for module '%1%'", module_name);
        }

        registerActions(metadata);
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata of module %1%: %2%",
                  module_name, e.what());
        std::string err { "invalid metadata of module " + module_name };
        throw Module::LoadingError { err };
    }
}

ExternalModule::ExternalModule(const std::string& path,
                               const std::string& spool_dir)
        : path_ { path },
          config_ { "{}" },
          storage_ { spool_dir }
{
    fs::path module_path { path };
    module_name = module_path.stem().string();
    auto metadata = getModuleMetadata();

    try {
       registerActions(metadata);
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata of module %1%: %2%",
                  module_name, e.what());
        std::string err { "invalid metadata of module " + module_name };
        throw Module::LoadingError { err };
    }
}

void ExternalModule::validateConfiguration()
{
    if (config_validator_.includesSchema(module_name)) {
        config_validator_.validate(config_, module_name);
    } else {
        LOG_DEBUG("The '%1%' configuration will not be validated; no JSON "
                  "schema is available", module_name);
    }
}

//
// Static class functions
//

void ExternalModule::processOutputAndUpdateMetadata(ActionResponse& response)
{
    if (response.output.std_out.empty()) {
        LOG_TRACE("Obtained no results on stdout for the %1%",
                  response.prettyRequestLabel());
    } else {
        LOG_TRACE("Results on stdout for the %1%: %2%",
                  response.prettyRequestLabel(), response.output.std_out);
    }

    if (response.output.exitcode != EXIT_SUCCESS) {
        LOG_TRACE("Execution failure (exit code %1%) for the %2%%3%",
                  response.output.exitcode, response.prettyRequestLabel(),
                  (response.output.std_err.empty()
                    ? ""
                    : "; stderr:\n" + response.output.std_err));
    } else if (!response.output.std_err.empty()) {
        LOG_TRACE("Output on stderr for the %1%:\n%2%",
                  response.prettyRequestLabel(), response.output.std_err);
    }

    try {
        // Ensure output format is valid JSON by instantiating
        // JsonContainer (NB: its ctor does not accept empty strings)
        lth_jc::JsonContainer results {
            (response.output.std_out.empty() ? "null" : response.output.std_out) };
        response.setValidResultsAndEnd(std::move(results));
    } catch (lth_jc::data_parse_error& e) {
        LOG_DEBUG("Obtained invalid JSON on stdout for the %1%; (validation "
                  "error: %2%); stdout:\n%3%",
                  response.prettyRequestLabel(), e.what(), response.output.std_out);
        auto execution_error =
            (boost::format("The task executed for the %1% returned invalid "
                           "JSON on stdout - stderr:%2%")
                % response.prettyRequestLabel()
                % (response.output.std_err.empty()
                        ? " (empty)"
                        : '\n' + response.output.std_err))
            .str();
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
    auto exec =
#ifdef _WIN32
        lth_exec::execute("cmd.exe", { "/c", path_, "metadata" },
#else
        lth_exec::execute(path_, { "metadata" },
#endif
            0, {lth_exec::execution_options::merge_environment});

    if (!exec.error.empty()) {
        LOG_ERROR("Failed to load the external module metadata from %1%: %2%",
                  path_, exec.error);
        throw Module::LoadingError { "failed to load external module metadata" };
    }

    lth_jc::JsonContainer metadata;

    try {
        metadata = lth_jc::JsonContainer { exec.output };
        LOG_DEBUG("External module %1%: metadata is valid JSON", module_name);
    } catch (lth_jc::data_error& e) {
        throw Module::LoadingError { std::string { "metadata is not in a valid "
                                        "JSON format: " } + e.what() };
    }

    try {
        metadata_validator_.validate(metadata, METADATA_SCHEMA_NAME);
        LOG_DEBUG("External module %1%: metadata validation OK", module_name);
    } catch (PCPClient::validation_error& e) {
        throw Module::LoadingError { std::string { "metadata validation failure: " }
                                     + e.what() };
    }

    return metadata;
}

void ExternalModule::registerConfiguration(const lth_jc::JsonContainer& config_metadata)
{
    try {
        PCPClient::Schema configuration_schema { module_name, config_metadata };
        LOG_DEBUG("Registering module config schema for '%1%'", module_name);
        config_validator_.registerSchema(configuration_schema);
    } catch (PCPClient::schema_error& e) {
        LOG_ERROR("Failed to parse the configuration schema of module '%1%': %2%",
                  module_name, e.what());
        std::string err { "invalid configuration schema of module " + module_name };
        throw Module::LoadingError { err };
    }
}

void ExternalModule::registerActions(const lth_jc::JsonContainer& metadata)
{
    for (auto& action
            : metadata.get<std::vector<lth_jc::JsonContainer>>(
                METADATA_ACTIONS_ENTRY))
        registerAction(action);
}

// Register the specified action after ensuring that the input and
// output schemas are valid JSON (i.e. we can instantiate Schema).
void ExternalModule::registerAction(const lth_jc::JsonContainer& action)
{
    // NOTE(ale): name, input, and output are required action entries
    auto action_name = action.get<std::string>("name");
    LOG_DEBUG("Validating action '%1% %2%'", module_name, action_name);

    try {
        auto input_schema_json = action.get<lth_jc::JsonContainer>("input");
        PCPClient::Schema input_schema { action_name, input_schema_json };

        auto results_schema_json = action.get<lth_jc::JsonContainer>("results");
        PCPClient::Schema results_schema { action_name, results_schema_json };

        // Metadata schemas are valid JSON; store metadata
        LOG_DEBUG("Action '%1% %2%' has been validated", module_name, action_name);
        actions.push_back(action_name);
        input_validator_.registerSchema(input_schema);
        results_validator_.registerSchema(results_schema);
    } catch (PCPClient::schema_error& e) {
        LOG_ERROR("Failed to parse metadata schemas of action '%1% %2%': %3%",
                  module_name, action_name, e.what());
        std::string err { "invalid schemas of '" + module_name + " "
                          + action_name + "'" };
        throw Module::LoadingError { err };
    } catch (lth_jc::data_error& e) {
        LOG_ERROR("Failed to retrieve metadata schemas of action '%1% %2%': %3%",
                  module_name, action_name, e.what());
        std::string err { "invalid metadata of '" + module_name + " "
                          + action_name + "'" };
        throw Module::LoadingError { err };
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

    LOG_INFO("Executing the %1%", request.prettyLabel());
    LOG_TRACE("Input for the %1%: %2%", request.prettyLabel(), action_args);

    auto exec = lth_exec::execute(
#ifdef _WIN32
        "cmd.exe", { "/c", path_, action_name },
#else
        path_, { action_name },
#endif
        action_args,                           // args
        std::map<std::string, std::string>(),  // environment
        0,                                     // timeout
        { lth_exec::execution_options::merge_environment });  // options

    response.output = ActionOutput { exec.exit_code, exec.output, exec.error };
    ExternalModule::processOutputAndUpdateMetadata(response);
    return response;
}

ActionResponse ExternalModule::callNonBlockingAction(const ActionRequest& request)
{
    ActionResponse response { ModuleType::External, request };
    auto action_name = request.action();
    auto input_txt = getActionArguments(request);
    fs::path results_dir_path { request.resultsDir() };
    auto out_file = (results_dir_path / "stdout").string();
    auto err_file = (results_dir_path / "stderr").string();

    LOG_INFO("Starting a task for the %1%; stdout and stderr will be stored in %2%",
             request.prettyLabel(), request.resultsDir());
    LOG_TRACE("Input for the %1%: %2%", request.prettyLabel(), input_txt);

    auto exec = lth_exec::execute(
#ifdef _WIN32
        "cmd.exe", { "/c", path_, action_name },
#else
        path_, { action_name },
#endif
        input_txt,  // input
        std::map<std::string, std::string>(),  // environment
        [results_dir_path](size_t pid) {
            auto pid_file = (results_dir_path / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file);
        },          // pid callback
        0,          // timeout
        { lth_exec::execution_options::merge_environment });  // options

    if (exec.exit_code == EXTERNAL_MODULE_FILE_ERROR_EC) {
        // This is unexpected. The output of the task will not be
        // available for future transaction status requests; we cannot
        // provide a reliable ActionResponse.
        LOG_WARNING("The task process failed to write output on file for the %1%; "
                    "stdout: %2%; stderr: %3%",
                    request.prettyLabel(),
                    (exec.output.empty() ? "(empty)" : exec.output),
                    (exec.error.empty() ? "(empty)" : exec.error));
        throw Module::ProcessingError { "failed to write output on file" };
    }

    // Stdout / stderr output is on file; read it
    response.output = storage_.getOutput(request.transactionId(), exec.exit_code);
    ExternalModule::processOutputAndUpdateMetadata(response);
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
