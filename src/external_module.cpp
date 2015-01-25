#include "src/external_module.h"
#include "src/schemas.h"
#include "src/action.h"
#include "src/errors.h"
#include "src/log.h"
#include "src/file_utils.h"
#include "src/timer.h"
#include "src/uuid.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>

LOG_DECLARE_NAMESPACE("external_module");

namespace CthunAgent {

//
// Free functions
//

// Execute binaries and get output and errors

void runCommand(const std::string& exec, std::vector<std::string> args,
                std::string stdin, std::string &stdout, std::string &stderr) {
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

// Perform delayed actions

void delayedAction(Message request,
                   std::string job_id,
                   std::string module_path,
                   std::string results_dir) {
    // Get request parameters
    auto request_id = request.get<std::string>("id");
    auto module_name = request.get<std::string>("data", "module");
    auto action_name = request.get<std::string>("data", "action");
    auto input = request.get<DataContainer>("data", "params");

    // Initialize result files
    DataContainer status {};
    status.set<std::string>(module_name, "module");
    status.set<std::string>(action_name, "action");

    if (!input.toString().empty()) {
        status.set<std::string>(input.toString(), "input");
    } else {
        status.set<std::string>("none", "input");
    }

    status.set<std::string>("running", "status");
    status.set<std::string>("0", "duration");

    FileUtils::writeToFile(status.toString() + "\n", results_dir + "/status");
    FileUtils::writeToFile("", results_dir + "/stdout");
    FileUtils::writeToFile("", results_dir + "/stderr");

    // Prepare and run the command
    std::string stdin = input.toString();
    std::string stdout;
    std::string stderr;
    CthunAgent::Timer timer;

    runCommand(module_path, { module_path, action_name }, stdin, stdout, stderr);

    status.set<std::string>(std::to_string(timer.elapsedSeconds()) + "s",
                            "duration");

    // Validate (creating a DataContainer validates the json output)
    try {
        DataContainer result { stdout };
    } catch (message_parse_error) {
        // TODO(ale): report outcome (perhaps with a 'success' field)
        stderr = "ERROR: failed to validate the '" + module_name + " "
                 + action_name + "' output\n" + stderr;
    }

    // Write results to files
    FileUtils::writeToFile(stdout + "\n", results_dir + "/stdout");

    if (!stderr.empty()) {
        FileUtils::writeToFile(stderr + "\n", results_dir + "/stderr");
    }

    status.set<std::string>("completed", "status");
    FileUtils::writeToFile(status.toString() + "\n", results_dir + "/status");
}

//
// ExternalModule
//

ExternalModule::ExternalModule(const std::string& path) : path_(path) {
    boost::filesystem::path module_path { path };
    module_name = module_path.filename().string();

    auto metadata = validateModuleAndGetMetadata_();

    for (auto action : metadata.get<std::vector<DataContainer>>("actions")) {
        validateAndDeclareAction_(action);
    }
}

DataContainer ExternalModule::callAction(const std::string& action_name,
                                         const Message& request) {
    // TODO(ale): consider moving this up to the Module class (enable
    // blocking/non-blocking requests for a given module action pair)

    if (actions[action_name].isDelayed()) {
        auto job_id = UUID::getUUID();
        LOG_DEBUG("Delayed action execution requested. Creating job " \
                  "with ID %1%", job_id);
        return executeDelayedAction(action_name, request, job_id);
    } else {
        return callBlockingAction(action_name, request);
    }
}

DataContainer ExternalModule::callBlockingAction(const std::string& action_name,
                                                 const Message& request) {
    std::string stdin = request.get<DataContainer>("data", "params").toString();
    std::string stdout;
    std::string stderr;
    LOG_INFO("About to execute '%1% %2%' - stdin: %3%",
             module_name, action_name, stdin);

    runCommand(path_, { path_, action_name }, stdin, stdout, stderr);

    LOG_INFO("stdout: %1%", stdout);
    LOG_INFO("stderr: %1%", stderr);

    return DataContainer { (stdout.empty() ? "{}" : stdout) };
}

DataContainer ExternalModule::executeDelayedAction(const std::string& action_name,
                                                   const Message& request,
                                                   const std::string& job_id) {
    DataContainer input { request.get<DataContainer>("data", "params") };

    // check if the output directory exists. If it doesn't create it
    if (!FileUtils::fileExists(spool_dir_)) {
        LOG_INFO("%1% directory does not exist. Creating.", spool_dir_);
        if (!FileUtils::createDirectory(spool_dir_)) {
            throw message_processing_error { "failed to create directory "
                                             + spool_dir_ };
        }
    }

    std::string results_dir { spool_dir_ + job_id };

    if (!FileUtils::fileExists(results_dir)) {
        LOG_INFO("Creating result directory for delayed action '%1% %2%' %3%",
                 module_name, action_name, job_id);
        if (!FileUtils::createDirectory(results_dir)) {
            throw message_processing_error { "failed to create directory "
                                             + results_dir };
        }
    }

    LOG_INFO("Starting delayed action '%1% %2%' %3%",
             module_name, action_name, job_id);

    // TODO(ale): we must manage the thread lifecycle (no background)
    // to avoid possible delays when the user quits the program;
    // possible alternatives: capture signals and call std::terminate
    // (i.e. mimic the old vector RIIA); future & timeout; thread pool

    // start thread
    try {
        std::thread(&delayedAction,
                    Message(request),
                    job_id,
                    std::string(path_),
                    results_dir).detach();
    } catch (std::exception& e) {
        LOG_ERROR("Failed to spawn '%1% %2%' thread: %3%",
                  module_name, action_name, e.what());
        throw message_processing_error { "failed to create the delayed action thread" };
    }

    // return response with the job id
    DataContainer response_output {};
    response_output.set<std::string>("Requested excution of action: " + action_name,
                                     "status");
    response_output.set<std::string>(job_id, "id");
    return response_output;
}

//
// Private methods
//

const DataContainer ExternalModule::validateModuleAndGetMetadata_() {
    valijson::Schema metadata_schema = Schemas::external_action_metadata();
    std::string metadata_txt;
    std::string error;

    runCommand(path_, { path_, "metadata" }, "", metadata_txt, error);

    if (!error.empty()) {
        LOG_ERROR("Failed to load external module %1%: %2%", path_, error);
        throw module_error { "failed to load external module" };
    }

    DataContainer metadata { metadata_txt };

    std::vector<std::string> errors;
    if (!metadata.validate(metadata_schema, errors)) {
        LOG_ERROR("Invalid module metadata");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw module_error { "metadata did not match schema" };
    }

    LOG_INFO("External module %1%: metadata validation OK", module_name);

    return metadata;
}

void ExternalModule::validateAndDeclareAction_(const DataContainer& action) {
    std::string action_name { action.get<std::string>("name") };
    LOG_INFO("Validating action %1%", action_name);
    valijson::Schema input_schema;
    valijson::Schema output_schema;

    // TODO(ploubser): This doesn't fit well with the Data abstraction.
    // Should this go in the object?

    // TODO(ale): unit tests for the validation once we move its logic

    valijson::SchemaParser parser;
    rapidjson::Value input  { action.get<rapidjson::Value>("input") };
    rapidjson::Value output { action.get<rapidjson::Value>("output") };

    valijson::adapters::RapidJsonAdapter input_doc_schema(input);
    valijson::adapters::RapidJsonAdapter output_doc_schema(output);

    try {
        parser.populateSchema(input_doc_schema, input_schema);
    } catch (...) {
        LOG_ERROR("Failed to parse input schema of %1%", action_name);
        throw module_error { "invalid input schema of " + action_name };
    }

    std::string behaviour { action.get<std::string>("behaviour") };

    if (!behaviour.empty()) {
        if (behaviour.compare("interactive") == 0) {
            LOG_DEBUG("Found interactive action: %1%", action_name);
        } else if (behaviour.compare("delayed") == 0) {
            LOG_DEBUG("Found delayed action: %1%", action_name);
        } else {
            LOG_ERROR("Invalid behaviour defined for action %1%: %2%",
                      action_name, behaviour);
            throw module_error { "invalid behavior of " + action_name };
        }
    } else {
        LOG_DEBUG("Found no behaviour for action %1%; using 'interactive'",
                  action_name);
        behaviour = "interactive";
    }

    try {
        parser.populateSchema(output_doc_schema, output_schema);
    } catch (...) {
        LOG_ERROR("Failed to parse output schema of %1%", action_name);
        throw module_error { "invalid output schema of " + action_name };
    }

    LOG_INFO("Action %1% of external module %2%: validation OK",
             action_name, module_name);
    actions[action.get<std::string>("name")] = Action {
        input_schema, output_schema, behaviour };
}


}  // namespace CthunAgent
