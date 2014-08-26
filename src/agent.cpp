#include "agent.h"
#include "modules/echo.h"
#include "log.h"
#include "schemas.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <json/json.h>

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


Agent::Agent() {
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);
    // the metadata schema
    valijson::Schema metadata_schema = Schemas::external_action_metadata();


    boost::filesystem::path module_path { "modules" };
    boost::filesystem::directory_iterator end;
    for (auto file = boost::filesystem::directory_iterator(module_path); file != end; ++file) {
        if (!boost::filesystem::is_directory(file->status())) {
            BOOST_LOG_TRIVIAL(info) << file->path().filename().string();

            std::string metadata;
            std::string error;
            run_command(file->path().string(), { "metadata" }, "", metadata, error);

            Json::Value document;
            Json::Reader reader;
            if (!reader.parse(metadata, document)) {
                BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
                continue;
            }

            std::vector<std::string> errors;
            if (!Schemas::validate(document, metadata_schema, errors)) {
                BOOST_LOG_TRIVIAL(error) << "Validation failed";
                for (auto error : errors) {
                    BOOST_LOG_TRIVIAL(error) << "    " << error;
                }
                continue;
            }
            BOOST_LOG_TRIVIAL(info) << "validation OK";

            std::unique_ptr<Module> loading(new Module);
            loading->name = file->path().filename().string();
            const Json::Value actions = document["actions"];
            for (auto action : actions) {
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
                    continue;
                }

                try {
                    parser.populateSchema(output_doc_schema, output_schema);
                } catch (...) {
                    BOOST_LOG_TRIVIAL(info) << "Failed to parse error schema.";
                    continue;
                }

                loading->actions[action["name"].asString()] = Action { input_schema, output_schema };
            }

            modules_[loading->name] = std::move(loading);
        }
    }
}

void Agent::run(std::string module, std::string action) {
    Action do_this = modules_[module]->actions[action];

    Json::Reader reader;
    Json::Value input;

    BOOST_LOG_TRIVIAL(info) << "loading stdin";

    if (!reader.parse(std::cin, input)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "validating input for " << module << " " << action;
    std::vector<std::string> errors;
    if (!Schemas::validate(input, do_this.input_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        return;
    }

    std::string stdin = input.toStyledString();
    std::string stdout;
    std::string stderr;
    BOOST_LOG_TRIVIAL(info) << stdin;

    run_command("modules/" + module, { module, action }, stdin, stdout, stderr);
    BOOST_LOG_TRIVIAL(info) << "stdout " << stdout;
    BOOST_LOG_TRIVIAL(info) << "stderr " << stderr;

    Json::Value output;
    if (!reader.parse(stdout, output)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "validating output for " << module << " " << action;
    if (!Schemas::validate(output, do_this.output_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "validated OK: " << output.toStyledString();
}


}  // namespace CthunAgent
