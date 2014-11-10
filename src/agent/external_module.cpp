#include "src/agent/external_module.h"
#include "src/agent/schemas.h"
#include "src/agent/action.h"
#include "src/common/log.h"
#include "src/common/file_utils.h"
#include "src/common/timer.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>

LOG_DECLARE_NAMESPACE("agent.external_module");

namespace Cthun {
namespace Agent {

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

    Json::Value document;
    Json::Reader reader;
    if (!reader.parse(metadata, document)) {
        LOG_ERROR("parse error: %1%", reader.getFormatedErrorMessages());
        throw "failed to parse metadata document";
    }

    std::vector<std::string> errors;
    if (!Schemas::validate(document, metadata_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        throw "metadata did not match schema";
    }

    LOG_INFO("validation OK");

    for (auto action : document["actions"]) {
        LOG_INFO("declaring action %1%", action["name"].asString());
        valijson::Schema input_schema;
        valijson::Schema output_schema;

        valijson::SchemaParser parser;
        valijson::adapters::JsonCppAdapter input_doc_schema(action["input"]);
        valijson::adapters::JsonCppAdapter output_doc_schema(action["output"]);

        try {
            parser.populateSchema(input_doc_schema, input_schema);
        } catch (...) {
            LOG_ERROR("failed to parse input schema");
            throw;
        }

        std::string behaviour = "interactive";

        if (!action["behaviour"].asString().empty()) {
            std::string behaviour_str = action["behaviour"].asString();
            if (behaviour_str.compare("interactive") == 0) {
                LOG_DEBUG("Found interactive action");
            } else if (behaviour_str.compare("delayed") == 0) {
                LOG_DEBUG("Found delayed action");
                behaviour = behaviour_str;
            } else {
                LOG_ERROR("Invalid behaviour defined for action: %1%", behaviour_str);
                throw;
            }
        }

        try {
            parser.populateSchema(output_doc_schema, output_schema);
        } catch (...) {
            LOG_ERROR("failed to parse error schema");
            throw;
        }

        actions[action["name"].asString()] = Action { input_schema, output_schema, behaviour };
    }
}

void ExternalModule::call_action(std::string action_name,
                                 const Json::Value& request,
                                 const Json::Value& input,
                                 Json::Value& output) {
    std::string stdin = input.toStyledString();
    std::string stdout;
    std::string stderr;
    LOG_INFO(stdin);

    run_command(path_, { path_, action_name }, stdin, stdout, stderr);
    LOG_INFO("stdout: %1%", stdout);
    LOG_INFO("stderr: %1%", stderr);

    Json::Reader reader;
    if (!reader.parse(stdout, output)) {
        LOG_INFO("parse error: %1%", reader.getFormatedErrorMessages());
        throw "error parsing json";
    }
}

void ExternalModule::call_delayed_action(std::string action_name,
                                         const Json::Value& request,
                                         const Json::Value& input,
                                         Json::Value& output,
                                         std::string job_id) {
    LOG_INFO("Starting delayed action with id: %1%", job_id);

    // TODO(ploubser): Refactor

    // check if the output directory exists. If it doesn't create it
    if (!Common::FileUtils::fileExists("/tmp/cthun_agent")) {
        LOG_INFO("/tmp/cthun_agent directory does not exist. Creating");
        if (!Common::FileUtils::createDirectory("/tmp/cthun_agent")) {
            LOG_ERROR("Failed to create /tmp/cthun_agent. Cannot start action.");
        }
    }

    std::string action_dir { "/tmp/cthun_agent/" + job_id };

    // create action specific result directory
    if (!Common::FileUtils::fileExists(action_dir)) {
        LOG_INFO("Creating result directory for action.");
        if (!Common::FileUtils::createDirectory(action_dir)) {
            LOG_ERROR("Failed to create " + action_dir + ". Cannot start action.");
        }
    }

    // create the exitcode file
    Json::Value status {};
    status["module"] = module_name;
    status["action"] = action_name;

    if (status["input"].asString().empty()) {
        status["input"] = "none";
    } else {
        status["input"] = input;
    }
    status["status"] = "running";
    status["duration"] = "0";

    Common::FileUtils::writeToFile(status.toStyledString() + "\n", action_dir + "/status");
    Common::FileUtils::writeToFile("", action_dir + "/stdout");
    Common::FileUtils::writeToFile("", action_dir + "/stderr");

    // prepare and run the command
    std::string stdin = input.toStyledString();
    std::string stdout;
    std::string stderr;

    Common::Timer timer;
    run_command(path_, { path_, action_name }, stdin, stdout, stderr);
    status["duration"] = std::to_string(timer.elapsedSeconds()) + "s";

    Json::Reader reader;
    if (!reader.parse(stdout, output)) {
        LOG_INFO("parse error: %1%", reader.getFormatedErrorMessages());
        throw "error parsing json";
    }

    status["status"] = "completed";
    Common::FileUtils::writeToFile(stdout + "\n", action_dir + "/stdout");
    if (!stderr.empty()) {
        Common::FileUtils::writeToFile(stderr + "\n", action_dir + "/stderr");
    }
    Common::FileUtils::writeToFile(status.toStyledString() + "\n", action_dir + "/status");
}

}  // namespace Agent
}  // namespace Cthun
