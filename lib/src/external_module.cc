#include <cthun-agent/external_module.hpp>
#include <cthun-agent/errors.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.external_module"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <atomic>
#include <memory>  // shared_ptr

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

namespace CthunAgent {

static const std::string METADATA_SCHEMA_NAME { "external_module_metadata" };
static const std::string ACTION_SCHEMA_NAME { "action_metadata" };

//
// Free functions
//

// TODO(ale): move this to leatherman
// Execute binaries and get output and errors
void runCommand(const std::string& exec,
                std::vector<std::string> args,
                std::string stdin,
                std::string &stdout,
                std::string &stderr) {
    boost::process::context context;
    context.stdin_behavior = boost::process::capture_stream();
    context.stdout_behavior = boost::process::capture_stream();
    context.stderr_behavior = boost::process::capture_stream();
    context.environment = boost::process::self::get_environment();
    boost::process::child child = boost::process::launch(exec, args, context);

    boost::process::postream &in = child.get_stdin();
    in << stdin;
    in.close();

    boost::process::pistream &out = child.get_stdout();
    std::string line;
    while (std::getline(out, line)) {
        stdout += line;
    }

    boost::process::pistream &err = child.get_stderr();
    while (std::getline(err, line)) {
        stderr += line;
    }

    child.wait();
}

// Provides the metadata validator
CthunClient::Validator getMetadataValidator() {
    // Metadata schema
    CthunClient::Schema metadata_schema { METADATA_SCHEMA_NAME,
                                          CthunClient::ContentType::Json };
    using T_C = CthunClient::TypeConstraint;
    metadata_schema.addConstraint("description", T_C::String, true);
    metadata_schema.addConstraint("actions", T_C::Array, true);

    // 'actions' is an array of actions; define the action sub_schema
    CthunClient::Schema action_schema { ACTION_SCHEMA_NAME,
                                        CthunClient::ContentType::Json };
    action_schema.addConstraint("description", T_C::String, false);
    action_schema.addConstraint("name", T_C::String, true);
    action_schema.addConstraint("input", T_C::Object, true);
    action_schema.addConstraint("output", T_C::Object, true);

    metadata_schema.addConstraint("actions", action_schema, false);

    CthunClient::Validator validator {};
    validator.registerSchema(metadata_schema);
    return validator;
}

//
// Public interface
//

ExternalModule::ExternalModule(const std::string& path) : path_ { path } {
    boost::filesystem::path module_path { path };
    module_name = module_path.filename().string();
    auto metadata = getMetadata();

    for (auto& action_metadata :
            metadata.get<std::vector<lth_jc::JsonContainer>>("actions")) {
        registerAction(action_metadata);
    }
}

//
// Private interface
//

// Metadata validator (static member)
const CthunClient::Validator ExternalModule::metadata_validator_ {
        getMetadataValidator() };


// Retrieve and validate the module metadata
const lth_jc::JsonContainer ExternalModule::getMetadata() {
    std::string metadata_txt;
    std::string error;

    runCommand(path_, { path_, "metadata" }, "", metadata_txt, error);

    if (!error.empty()) {
        LOG_ERROR("Failed to load the external module metadata from %1%: %2%",
                  path_, error);
        throw Module::Error { "failed to load external module metadata" };
    }

    lth_jc::JsonContainer metadata { metadata_txt };

    try {
        metadata_validator_.validate(metadata, METADATA_SCHEMA_NAME);
        LOG_INFO("External module %1%: metadata validation OK", module_name);
    } catch (CthunClient::validation_error& e) {
        throw Module::Error { std::string("metadata validation failure: ")
                              + e.what() };
    }

    return metadata;
}

// Register the specified action after ensuring that the input and
// output schemas are valid JSON (i.e. we can instantiate Schema).
void ExternalModule::registerAction(const lth_jc::JsonContainer& action) {
    auto action_name = action.get<std::string>("name");
    LOG_INFO("Validating action '%1% %2%'", module_name, action_name);

    try {
        auto input_schema_json = action.get<lth_jc::JsonContainer>("input");
        CthunClient::Schema input_schema { action_name, input_schema_json };

        auto output_schema_json = action.get<lth_jc::JsonContainer>("output");
        CthunClient::Schema output_schema { action_name, output_schema_json };

        // Input & output schemas are valid JSON; store metadata
        LOG_INFO("Action '%1% %2%' has been validated", module_name, action_name);
        actions.push_back(action_name);
        input_validator_.registerSchema(input_schema);
        output_validator_.registerSchema(output_schema);
    } catch (CthunClient::schema_error& e) {
        LOG_ERROR("Failed to parse input/output schemas of action '%1% %2%': %3%",
                  module_name, action_name, e.what());
        throw Module::Error { std::string("invalid schemas of '" + module_name
                                          + " " + action_name + "'") };
    }
}

ActionOutcome ExternalModule::callAction(const ActionRequest& request) {
    std::string stdout {};
    std::string stderr {};
    auto& action_name = request.action();

    LOG_INFO("About to execute '%1% %2%' - request input: %3%",
             module_name, action_name, request.requestTxt());

    runCommand(path_, { path_, action_name }, request.requestTxt(),
               stdout, stderr);

    if (stdout.empty()) {
        LOG_DEBUG("'%1% %2%' produced no output", module_name, action_name);
    } else {
        LOG_DEBUG("'%1% %2%' output: %3%", module_name, action_name, stdout);
    }

    if (!stderr.empty()) {
        LOG_ERROR("'%1% %2%' error: %3%", module_name, action_name, stderr);
    }

    // Ensure output format is valid JSON by instantiating JsonContainer
    lth_jc::JsonContainer results {};

    try {
        results = lth_jc::JsonContainer { stdout };
    } catch (lth_jc::data_parse_error& e) {
        LOG_ERROR("'%1% %2%' output is not valid JSON: %3%",
                  module_name, action_name, e.what());
        std::string err_msg { "'" + module_name + " " + action_name + "' "
                              "returned invalid JSON - stderr: " + stderr };
        throw request_processing_error { err_msg };
    }

    return ActionOutcome { stderr, stdout, results };
}

}  // namespace CthunAgent
