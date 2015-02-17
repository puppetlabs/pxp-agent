#include "src/schemas.h"

#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

namespace CthunAgent {

namespace V_C = valijson::constraints;

// Auxiliary function

valijson::Schema getActionSubschema_() {
    // some common schema constants to reduce typing
    V_C::TypeConstraint json_type_object { V_C::TypeConstraint::kObject };
    V_C::TypeConstraint json_type_string { V_C::TypeConstraint::kString };

    // action sub-schema
    valijson::Schema schema;
    V_C::PropertiesConstraint::PropertySchemaMap properties;
    V_C::PropertiesConstraint::PropertySchemaMap pattern_properties;
    V_C::RequiredConstraint::RequiredProperties required_properties;

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
    schema.addConstraint(new V_C::PropertiesConstraint(properties,
                                                       pattern_properties));

    // specify the required properties
    schema.addConstraint(new V_C::RequiredConstraint(required_properties));

    return schema;
}

//
// api
//

bool Schemas::validate(const rapidjson::Value& document,
                       const valijson::Schema& schema,
                       std::vector<std::string>& errors) {
    valijson::Validator validator { schema };
    valijson::adapters::RapidJsonAdapter adapted_document { document };
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

valijson::Schema Schemas::getExternalActionMetadataSchema() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    V_C::TypeConstraint json_type_object { V_C::TypeConstraint::kObject };
    V_C::TypeConstraint json_type_string { V_C::TypeConstraint::kString };
    V_C::TypeConstraint json_type_array { V_C::TypeConstraint::kArray };

    V_C::PropertiesConstraint::PropertySchemaMap properties;
    V_C::PropertiesConstraint::PropertySchemaMap pattern_properties;
    V_C::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    // description is required
    required_properties.insert("description");
    properties["description"].addConstraint(json_type_string);

    // actions is an array of the action sub-schema
    required_properties.insert("actions");
    properties["actions"].addConstraint(json_type_array);

    valijson::Schema action_schema = getActionSubschema_();
    V_C::ItemsConstraint items_of_type_action_schema { action_schema };
    properties["actions"].addConstraint(items_of_type_action_schema);

    // constrain the properties to just those in the properies and
    // pattern_properties maps
    schema.addConstraint(new V_C::PropertiesConstraint(properties,
                                                       pattern_properties));

    // specify the required properties
    schema.addConstraint(new V_C::RequiredConstraint(required_properties));

    return schema;
}

valijson::Schema Schemas::getEnvelopeSchema() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    V_C::TypeConstraint json_type_object { V_C::TypeConstraint::kObject };
    V_C::TypeConstraint json_type_string { V_C::TypeConstraint::kString };
    V_C::TypeConstraint json_type_array { V_C::TypeConstraint::kArray };
    V_C::TypeConstraint json_type_integer { V_C::TypeConstraint::kInteger };

    V_C::PropertiesConstraint::PropertySchemaMap properties;
    V_C::PropertiesConstraint::PropertySchemaMap pattern_properties;
    V_C::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

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

    required_properties.insert("data_schema");
    // TODO(richardc): maybe this has a set form
    properties["data_schema"].addConstraint(json_type_string);

    // constrain the properties to just those in the properies and
    // pattern_properties maps
    schema.addConstraint(new V_C::PropertiesConstraint(properties,
                                                       pattern_properties));

    // specify the required properties
    schema.addConstraint(new V_C::RequiredConstraint(required_properties));

    return schema;
}

valijson::Schema Schemas::getDataSchema() {
    valijson::Schema schema;

    // some common schema constants to reduce typing
    V_C::TypeConstraint json_type_object { V_C::TypeConstraint::kObject };
    V_C::TypeConstraint json_type_string { V_C::TypeConstraint::kString };

    V_C::PropertiesConstraint::PropertySchemaMap properties;
    V_C::PropertiesConstraint::PropertySchemaMap pattern_properties;
    V_C::RequiredConstraint::RequiredProperties required_properties;

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
    schema.addConstraint(new V_C::PropertiesConstraint(properties,
                                                       pattern_properties));

    // specify the required properties
    schema.addConstraint(new V_C::RequiredConstraint(required_properties));

    return schema;
}

}  // namespace CthunAgent
