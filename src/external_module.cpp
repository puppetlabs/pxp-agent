#include "external_module.h"
#include "schemas.h"
#include "log.h"
#include "action.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>


namespace CthunAgent {

void run_command(std::string exec, std::vector<std::string> args, std::string stdin, std::string &stdout, std::string &stderr) {
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

    boost::process::status status = child.wait();
}

ExternalModule::ExternalModule(std::string path) : path_(path) {
    boost::filesystem::path module_path { path };

    name = module_path.filename().string();

    valijson::Schema metadata_schema = Schemas::external_action_metadata();

    std::string metadata;
    std::string error;
    run_command(path, { "metadata" }, "", metadata, error);

    Json::Value document;
    Json::Reader reader;
    if (!reader.parse(metadata, document)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        throw;
    }

    std::vector<std::string> errors;
    if (!Schemas::validate(document, metadata_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        throw;
    }

    BOOST_LOG_TRIVIAL(info) << "validation OK";

    std::unique_ptr<Module> loading(new Module);

    for (auto action : document["actions"]) {
        BOOST_LOG_TRIVIAL(info) << "declaring action " << action["name"].asString();
        valijson::Schema input_schema;
        valijson::Schema output_schema;

        valijson::SchemaParser parser;
        valijson::adapters::JsonCppAdapter input_doc_schema(action["input"]);
        valijson::adapters::JsonCppAdapter output_doc_schema(action["output"]);

        try {
            parser.populateSchema(input_doc_schema, input_schema);
        } catch (...) {
            BOOST_LOG_TRIVIAL(info) << "Failed to parse input schema.";
            throw;
        }

        try {
            parser.populateSchema(output_doc_schema, output_schema);
        } catch (...) {
            BOOST_LOG_TRIVIAL(info) << "Failed to parse error schema.";
            throw;
        }

        actions[action["name"].asString()] = Action { input_schema, output_schema };
    }
}

void ExternalModule::call_action(const std::string action, const Json::Value& input, Json::Value& output) {
    std::string stdin = input.toStyledString();
    std::string stdout;
    std::string stderr;
    BOOST_LOG_TRIVIAL(info) << stdin;

    run_command(path_, { path_, action }, stdin, stdout, stderr);
    BOOST_LOG_TRIVIAL(info) << "stdout " << stdout;
    BOOST_LOG_TRIVIAL(info) << "stderr " << stderr;

    Json::Reader reader;
    if (!reader.parse(stdout, output)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        throw;
    }
}

}
