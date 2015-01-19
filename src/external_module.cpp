#include "src/external_module.h"
#include "src/schemas.h"
#include "src/action.h"
#include "src/errors.h"
#include "src/log.h"
#include "src/file_utils.h"
#include "src/timer.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>

LOG_DECLARE_NAMESPACE("external_module");

namespace CthunAgent {

void run_command(std::string exec, std::vector<std::string> args,
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

ExternalModule::ExternalModule(std::string path) : path_(path) {
    boost::filesystem::path module_path { path };

    module_name = module_path.filename().string();

    valijson::Schema metadata_schema = Schemas::external_action_metadata();

    std::string metadata;
    std::string error;
    run_command(path, { path, "metadata" }, "", metadata, error);

    if (!error.empty()) {
        LOG_ERROR("Error while trying to load external module: %1%", path);
        LOG_ERROR(error);
        throw module_error { "failed to load external module" };
    }

    DataContainer document { metadata };

    std::vector<std::string> errors;
    if (!document.validate(metadata_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw module_error { "metadata did not match schema" };
    }

    LOG_INFO("validation OK");

    for (auto action : document.get<std::vector<DataContainer>>("actions")) {
        std::string action_name { action.get<std::string>("name") };
        LOG_INFO("declaring action %1%", action_name);
        valijson::Schema input_schema;
        valijson::Schema output_schema;

        // TODO(ploubser): This doesn't fit well with the Data abstraction.
        // Should this go in the object?

        // TODO(ale): unit tests for the validation once we move its logic

        valijson::SchemaParser parser;
        rapidjson::Value input { action.get<rapidjson::Value>("input") };
        rapidjson::Value output { action.get<rapidjson::Value>("output") };

        valijson::adapters::RapidJsonAdapter input_doc_schema(input);
        valijson::adapters::RapidJsonAdapter output_doc_schema(output);

        try {
            parser.populateSchema(input_doc_schema, input_schema);
        } catch (...) {
            LOG_ERROR("Failed to parse input schema of %1%", action_name);
            throw module_error { "invalid input schema of " + action_name };
        }

        std::string behaviour = "interactive";

        if (!action.get<std::string>("behaviour").empty()) {
            std::string behaviour_str = action.get<std::string>("behaviour");
            if (behaviour_str.compare("interactive") == 0) {
                LOG_DEBUG("Found interactive action: %1%", action_name);
            } else if (behaviour_str.compare("delayed") == 0) {
                LOG_DEBUG("Found delayed action: %1%", action_name);
                behaviour = behaviour_str;
            } else {
                LOG_ERROR("Invalid behaviour defined for action %1%: %2%",
                          action_name, behaviour_str);
                throw module_error { "invalid behavior of " + action_name };
            }
        }

        try {
            parser.populateSchema(output_doc_schema, output_schema);
        } catch (...) {
            LOG_ERROR("Failed to parse output schema of %1%", action_name);
            throw module_error { "invalid output schema of " + action_name };
        }

        actions[action.get<std::string>("name")] = Action {
            input_schema, output_schema, behaviour };
    }
}

DataContainer ExternalModule::call_action(std::string action_name,
                                          const Message& request,
                                          const DataContainer& input) {
    std::string stdin = input.toString();
    std::string stdout;
    std::string stderr;
    LOG_INFO(stdin);

    run_command(path_, { path_, action_name }, stdin, stdout, stderr);
    LOG_INFO("stdout: %1%", stdout);
    LOG_INFO("stderr: %1%", stderr);

    return DataContainer { stdout };
}

void ExternalModule::call_delayed_action(std::string action_name,
                                         const Message& request,
                                         const DataContainer& input,
                                         std::string job_id) {
    LOG_INFO("Starting delayed action with id: %1%", job_id);

    // check if the output directory exists. If it doesn't create it
    if (!FileUtils::fileExists("/tmp/cthun_agent")) {
        LOG_INFO("/tmp/cthun_agent directory does not exist. Creating.");
        if (!FileUtils::createDirectory("/tmp/cthun_agent")) {
            LOG_ERROR("Failed to create /tmp/cthun_agent. Cannot start action.");
        }
    }

    std::string action_dir { "/tmp/cthun_agent/" + job_id };

    // create job specific result directory
    if (!FileUtils::fileExists(action_dir)) {
        LOG_INFO("Creating result directory for action.");
        if (!FileUtils::createDirectory(action_dir)) {
            LOG_ERROR("Failed to create " + action_dir + ". Cannot start action.");
        }
    }

    // create the exitcode file
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

    FileUtils::writeToFile(status.toString() + "\n", action_dir + "/status");
    FileUtils::writeToFile("", action_dir + "/stdout");
    FileUtils::writeToFile("", action_dir + "/stderr");

    // prepare and run the command
    std::string stdin = input.toString();
    std::string stdout;
    std::string stderr;

    CthunAgent::Timer timer;
    run_command(path_, { path_, action_name }, stdin, stdout, stderr);
    status.set<std::string>(std::to_string(timer.elapsedSeconds()) + "s", "duration");

    // Creating a DataContainer validates that the output is json
    DataContainer result { stdout };

    status.set<std::string>("completed", "status");

    FileUtils::writeToFile(stdout + "\n", action_dir + "/stdout");
    if (!stderr.empty()) {
        FileUtils::writeToFile(stderr + "\n", action_dir + "/stderr");
    }
    FileUtils::writeToFile(status.toString() + "\n", action_dir + "/status");
}

}  // namespace CthunAgent
