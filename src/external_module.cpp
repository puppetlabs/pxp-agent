#include "src/external_module.h"
#include "src/action.h"
#include "src/errors.h"
#include "src/log.h"
#include "src/file_utils.h"
#include "src/timer.h"
#include "src/uuid.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <atomic>
#include <memory>  // shared_ptr

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

LOG_DECLARE_NAMESPACE("external_module");

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

// Task that performs delayed actions
void delayedAction(CthunClient::ParsedChunks parsed_chunks,
                   std::string job_id,
                   std::string module_path,
                   std::string results_dir,
                   std::shared_ptr<std::atomic<bool>> done) {
    // Get request info
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    auto module_name = parsed_chunks.data.get<std::string>("module");
    auto action_name = parsed_chunks.data.get<std::string>("action");
    auto request_input =
        parsed_chunks.data.get<CthunClient::DataContainer>("params");

    // Initialize result files (out, err, and status)
    CthunClient::DataContainer action_status {};
    action_status.set<std::string>("module", module_name);
    action_status.set<std::string>("action", action_name);
    action_status.set<std::string>("status", "running");
    action_status.set<std::string>("duration", "0 s");

    if (!request_input.toString().empty()) {
        action_status.set<std::string>("input", request_input.toString());
    } else {
        action_status.set<std::string>("input", "none");
    }

    FileUtils::writeToFile("", results_dir + "/stdout");
    FileUtils::writeToFile("", results_dir + "/stderr");
    FileUtils::writeToFile(action_status.toString() + "\n",
                           results_dir + "/status");

    // Prepare and run the action command
    std::string stdin = request_input.toString();
    std::string stdout;
    std::string stderr;
    CthunAgent::Timer timer;
    runCommand(module_path, { module_path, action_name }, stdin, stdout, stderr);

    // Ensure output is valid JSON by instantiating DataContainer
    try {
        CthunClient::DataContainer output { stdout };
    } catch (CthunClient::parse_error) {
        // TODO(ale): report outcome (perhaps with a 'success' field)
        stderr = "ERROR: failed to validate the '" + module_name + " "
                 + action_name + "' output:\n" + stderr;
    }

    // Update result files
    std::string duration { std::to_string(timer.elapsedSeconds()) + " s" };
    action_status.set<std::string>("status", "completed");
    action_status.set<std::string>("duration", duration);

    FileUtils::writeToFile(stdout + "\n", results_dir + "/stdout");
    FileUtils::writeToFile(action_status.toString() + "\n",
                           results_dir + "/status");
    if (!stderr.empty()) {
        FileUtils::writeToFile(stderr + "\n", results_dir + "/stderr");
    }

    // Set the completion atomic flag
    *done = true;
}

// Provides the metadata validator

CthunClient::Validator getMetadataValidator_() {
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
    action_schema.addConstraint("behaviour", T_C::String, false);

    metadata_schema.addConstraint("actions", action_schema, false);

    CthunClient::Validator validator {};
    validator.registerSchema(metadata_schema);
    return validator;
}

//
// ExternalModule
//

// Metadata validator (static member)
const CthunClient::Validator ExternalModule::metadata_validator_ {
        getMetadataValidator_() };

ExternalModule::ExternalModule(const std::string& path)
        : spool_dir_ { Configuration::Instance().get<std::string>("spool-dir") },
          path_ { path },
          thread_container_ {} {
    boost::filesystem::path module_path { path };
    module_name = module_path.filename().string();
    thread_container_.setName(module_name);

    auto metadata = getMetadata_();

    for (auto& action_metadata :
            metadata.get<std::vector<CthunClient::DataContainer>>("actions")) {
        registerAction_(action_metadata);
    }
}

CthunClient::DataContainer ExternalModule::callAction(
                        const std::string& action_name,
                        const CthunClient::ParsedChunks& parsed_chunks) {
    // TODO(ale): consider moving this up to the Module class (enable
    // blocking/non-blocking requests for a given module action pair)

    if (actions[action_name].isDelayed()) {
        auto job_id = UUID::getUUID();
        LOG_DEBUG("Delayed action execution requested. Creating job " \
                  "with ID %1%", job_id);
        return executeDelayedAction(action_name, parsed_chunks, job_id);
    } else {
        return callBlockingAction(action_name, parsed_chunks);
    }
}

CthunClient::DataContainer ExternalModule::callBlockingAction(
                        const std::string& action_name,
                        const CthunClient::ParsedChunks& parsed_chunks) {
    std::string stdout {};
    std::string stderr {};
    auto request_input =
        parsed_chunks.data.get<CthunClient::DataContainer>("params").toString();

    LOG_INFO("About to execute '%1% %2%' - request input: %3%",
             module_name, action_name, request_input);

    runCommand(path_, { path_, action_name }, request_input, stdout, stderr);

    if (stdout.empty()) {
        LOG_INFO("'%1% %2%' produced no output", module_name, action_name);
    } else {
        LOG_INFO("'%1% %2%' output: %3%", module_name, action_name, stdout);
    }

    if (!stderr.empty()) {
        LOG_ERROR("'%1% %2%' error: %3%", module_name, action_name, stderr);
    }

    return CthunClient::DataContainer { (stdout.empty() ? "{}" : stdout) };
}

CthunClient::DataContainer ExternalModule::executeDelayedAction(
                        const std::string& action_name,
                        const CthunClient::ParsedChunks& parsed_chunks,
                        const std::string& job_id) {
    // check if the output directory exists. If it doesn't, create it
    if (!FileUtils::fileExists(spool_dir_)) {
        LOG_INFO("%1% directory does not exist. Creating.", spool_dir_);
        if (!FileUtils::createDirectory(spool_dir_)) {
            throw request_processing_error { "failed to create directory "
                                             + spool_dir_ };
        }
    }

    std::string results_dir { spool_dir_ + job_id };

    if (!FileUtils::fileExists(results_dir)) {
        LOG_INFO("Creating result directory for delayed action '%1% %2%', "
                 "id: %3%", module_name, action_name, job_id);
        if (!FileUtils::createDirectory(results_dir)) {
            throw request_processing_error { "failed to create directory "
                                             + results_dir };
        }
    }

    LOG_INFO("Starting delayed action '%1% %2%', id: %3%",
             module_name, action_name, job_id);

    // start thread
    try {
        std::shared_ptr<std::atomic<bool>> done {
            new  std::atomic<bool> { false } };

        thread_container_.add(std::thread(&delayedAction,
                                          parsed_chunks,
                                          job_id,
                                          std::string(path_),
                                          results_dir,
                                          done),
                              done);
    } catch (std::exception& e) {
        LOG_ERROR("Failed to spawn '%1% %2%' thread: %3%",
                  module_name, action_name, e.what());
        throw request_processing_error { "failed to create the delayed "
                                         "action thread" };
    }

    // return response with the job id
    CthunClient::DataContainer response_output {};
    response_output.set<std::string>("status",
            std::string("Requested excution of action: " + action_name));
    response_output.set<std::string>("id", static_cast<std::string>(job_id));

    return response_output;
}

//
// Private methods
//

// Retrieve and validate the module metadata
const CthunClient::DataContainer ExternalModule::getMetadata_() {
    std::string metadata_txt;
    std::string error;

    runCommand(path_, { path_, "metadata" }, "", metadata_txt, error);

    if (!error.empty()) {
        LOG_ERROR("Failed to load the external module metadata from %1%: %2%",
                  path_, error);
        throw module_error { "failed to load external module metadata" };
    }

    CthunClient::DataContainer metadata { metadata_txt };

    try {
        metadata_validator_.validate(metadata, METADATA_SCHEMA_NAME);
        LOG_INFO("External module %1%: metadata validation OK", module_name);
    } catch (CthunClient::validation_error& e) {
        throw module_error { std::string("metadata validation failure: ")
                             + e.what() };
    }

    return metadata;
}

// Register the specified action after: ensuring that the input and
// output schemas are valid JSON (i.e. we can instantiate Schema);
// ensuring that the action behaviour is known.
void ExternalModule::registerAction_(const CthunClient::DataContainer& action) {
    auto action_name = action.get<std::string>("name");
    LOG_INFO("Validating action '%1% %2%'", module_name, action_name);

    try {
        auto input_schema_json = action.get<CthunClient::DataContainer>("input");
        CthunClient::Schema input_schema { action_name, input_schema_json };

        auto output_schema_json = action.get<CthunClient::DataContainer>("output");
        CthunClient::Schema output_schema { action_name, output_schema_json };

        std::string behaviour { action.get<std::string>("behaviour") };

        if (!behaviour.empty()) {
            if (behaviour == "interactive") {
                LOG_DEBUG("The behaviour of action '%1% %2%' is interactive",
                          module_name, action_name);
            } else if (behaviour == "delayed") {
                LOG_DEBUG("The behaviour of action '%1% %2%' is interactive",
                          module_name, action_name);
            } else {
                LOG_ERROR("Unknown behaviour defined for action '%1% %2%': %3%",
                          module_name, action_name, behaviour);
                throw module_error { "unknown behavior of " + action_name };
            }
        } else {
            LOG_DEBUG("Found no behaviour for action '%1% %2%'; "
                      "using 'interactive'", module_name, action_name);
            behaviour = "interactive";
        }

        LOG_INFO("Action '%1% %2%' has been validated", module_name, action_name);
        actions[action_name] = Action { behaviour };
        input_validator_.registerSchema(input_schema);
        output_validator_.registerSchema(output_schema);
    } catch (CthunClient::schema_error& e) {
        LOG_ERROR("Failed to parse input/output schemas of action '%1% %2%': %3%",
                  module_name, action_name, e.what());
        throw module_error { std::string("invalid schemas of '" + module_name
                                         + " " + action_name + "'") };
    }
}

}  // namespace CthunAgent
