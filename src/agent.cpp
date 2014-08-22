#include "agent.h"
#include "modules/echo.h"
#include "log.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/process.hpp>

#include <json/json.h>


#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

namespace puppetlabs {
namespace cthun {

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

bool validate_against_schema(const Json::Value& document, const valijson::Schema& schema) {
    valijson::Validator validator(schema);
    valijson::adapters::JsonCppAdapter adapted_document(document);

    valijson::ValidationResults validation_results;
    if (!validator.validate(adapted_document, &validation_results)) {
        BOOST_LOG_TRIVIAL(error) << "Failed validation";
        valijson::ValidationResults::Error error;
        while (validation_results.popError(error)) {
            for (auto path : error.context) {
                BOOST_LOG_TRIVIAL(error) << path;
            }
            BOOST_LOG_TRIVIAL(error) << error.description;
        }
        return false;
    }
    return true;
}


Agent::Agent() {
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);

    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object(valijson::constraints::TypeConstraint::kObject);
    valijson::constraints::TypeConstraint json_type_string(valijson::constraints::TypeConstraint::kString);
    valijson::constraints::TypeConstraint json_type_array(valijson::constraints::TypeConstraint::kArray);

    // action sub-schema
    valijson::Schema action_schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap action_properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap action_pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties action_required;

    action_schema.addConstraint(json_type_object);

    action_properties["description"].addConstraint(json_type_string);

    action_required.insert("name");
    action_properties["name"].addConstraint(json_type_string);

    action_required.insert("input");
    action_properties["input"].addConstraint(json_type_object);

    action_required.insert("output");
    action_properties["output"].addConstraint(json_type_object);

    // constrain the properties to just those in the metadata_properies map
    action_schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                                    action_properties,
                                    action_pattern_properties));

    // specify the required properties
    action_schema.addConstraint(new valijson::constraints::RequiredConstraint(action_required));


    // the metadata schema
    valijson::Schema metadata_schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap metadata_properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap metadata_pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties metadata_required;

    metadata_schema.addConstraint(json_type_object);

    // description is required
    metadata_required.insert("description");
    metadata_properties["description"].addConstraint(json_type_string);

    // actions is an array of the action sub-schema
    metadata_required.insert("actions");
    metadata_properties["actions"].addConstraint(json_type_array);
    valijson::constraints::ItemsConstraint items_of_type_action_schema(action_schema);
    metadata_properties["actions"].addConstraint(items_of_type_action_schema);

    // constrain the properties to just those in the metadata_properies map
    metadata_schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                                      metadata_properties,
                                      metadata_pattern_properties));

    // specify the required properties
    metadata_schema.addConstraint(new valijson::constraints::RequiredConstraint(metadata_required));


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


            if (!validate_against_schema(document, metadata_schema)) {
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
    if (!validate_against_schema(input, do_this.input_schema)) {
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
    if (!validate_against_schema(output, do_this.output_schema)) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "validated OK: " << output.toStyledString();
}



}  // namespace cthun
}  // namespace puppetlabs
