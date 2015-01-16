#include "src/agent/schemas.h"

#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

// TODO(ale): log messages

namespace Cthun {
namespace Agent {

bool Schemas::validate(const rapidjson::Value& document,
                       const valijson::Schema& schema,
                       std::vector<std::string>& errors) {
    valijson::Validator validator(schema);
    valijson::adapters::RapidJsonAdapter adapted_document(document);

    valijson::ValidationResults validation_results;
    if (!validator.validate(adapted_document, &validation_results)) {
        valijson::ValidationResults::Error error;
        while (validation_results.popError(error)) {
            std::string path;
            for (auto component : error.context) {
                if (path.size()) {
                    path += ".";
                }
                path += component;
            }
            errors.push_back(path + ": " + error.description);
        }
        return false;
    }

    return true;
}

valijson::Schema action_subschema() {
    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject };
    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };

    // action sub-schema
    valijson::Schema schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    properties["description"].addConstraint(json_type_string);

    required_properties.insert("name");
    properties["name"].addConstraint(json_type_string);

    required_properties.insert("input");
    properties["input"].addConstraint(json_type_object);

    required_properties.insert("output");
    properties["output"].addConstraint(json_type_object);

    // Behaviour of execution defaults to interactive
    // TODO(ploubser): Find a better name for this
    properties["behaviour"].addConstraint(json_type_string);


    // constrain the properties to just those in the metadata_properies map
    schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                              properties,
                              pattern_properties));

    // specify the required properties
    schema.addConstraint(
        new valijson::constraints::RequiredConstraint(required_properties));

    return schema;
}

valijson::Schema Schemas::external_action_metadata() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject };
    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };
    valijson::constraints::TypeConstraint json_type_array {
        valijson::constraints::TypeConstraint::kArray };

    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    // description is required
    required_properties.insert("description");
    properties["description"].addConstraint(json_type_string);

    // actions is an array of the action sub-schema
    required_properties.insert("actions");
    properties["actions"].addConstraint(json_type_array);

    valijson::Schema action_schema = action_subschema();
    valijson::constraints::ItemsConstraint items_of_type_action_schema {
        action_schema };
    properties["actions"].addConstraint(items_of_type_action_schema);

    // constrain the properties to just those in the properies and
    // pattern_properties maps
    schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                             properties,
                             pattern_properties));

    // specify the required properties
    schema.addConstraint(
        new valijson::constraints::RequiredConstraint(required_properties));

    return schema;
}

valijson::Schema Schemas::network_message() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject };
    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };
    valijson::constraints::TypeConstraint json_type_array {
        valijson::constraints::TypeConstraint::kArray };
    valijson::constraints::TypeConstraint json_type_integer {
        valijson::constraints::TypeConstraint::kInteger };

    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    required_properties.insert("version");
    properties["version"].addConstraint(json_type_string);

    required_properties.insert("id");
    properties["id"].addConstraint(json_type_string);

    required_properties.insert("expires");
    // TODO(richardc): ISO 8061 formatted date string
    properties["expires"].addConstraint(json_type_string);

    required_properties.insert("sender");
    // TODO(richardc): endpoint identfier
    properties["sender"].addConstraint(json_type_string);

    required_properties.insert("endpoints");
    properties["endpoints"].addConstraint(json_type_array);
    // TODO(richardc): array of endpoint identifiers

    required_properties.insert("hops");
    properties["hops"].addConstraint(json_type_array);
    // TODO(richardc): array of 'hop' documents - define hop

    required_properties.insert("data_schema");
    // TODO(richardc): maybe this has a set form
    properties["data_schema"].addConstraint(json_type_string);

    // data is optional TODO(richardc): this may not be the best way
    // to mark something optional, as it could be a different json
    // primitive.  cnc schema will be an object though
    properties["data"].addConstraint(json_type_object);

    // constrain the properties to just those in the properies and
    // pattern_properties maps
    schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                             properties,
                             pattern_properties));

    // specify the required properties
    schema.addConstraint(
        new valijson::constraints::RequiredConstraint(required_properties));

    return schema;
}

valijson::Schema Schemas::cnc_data() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject };
    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };

    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    required_properties.insert("module");
    properties["module"].addConstraint(json_type_string);

    required_properties.insert("action");
    properties["action"].addConstraint(json_type_string);

    // params are optional
    // TODO(richardc): this may not be the best way
    // to mark something optional, as it could be a different json
    // primitive, say a simple scalar
    properties["params"] = valijson::Schema {};
    // .addConstraint(json_type_object);

    // constrain the properties to just those in the properies and
    // pattern_properties maps
    schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                             properties,
                             pattern_properties));

    // specify the required properties
    schema.addConstraint(
        new valijson::constraints::RequiredConstraint(required_properties));

    return schema;
}

}  // namespace Agent
}  // namespace Cthun
