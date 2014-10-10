#include "src/agent/external_module.h"
#include "src/agent/schemas.h"
#include "src/agent/action.h"
#include "src/common/log.h"

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

        try {
            parser.populateSchema(output_doc_schema, output_schema);
        } catch (...) {
            LOG_ERROR("failed to parse error schema");
            throw;
        }

        actions[action["name"].asString()] = Action { input_schema, output_schema };
    }
}

void ExternalModule::call_action(const std::string action_name,
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

}  // namespace Agent
}  // namespace Cthun
