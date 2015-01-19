#include "test/test.h"

#include "src/data_container.h"
#include "src/schemas.h"

#include <iostream>

// TODO(ploubser): Consider moving the validator into the DataContainer wrapper
#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

static const std::string JSON = "{\"foo\" : {\"bar\" : 2},"
                                " \"goo\" : 1,"
                                " \"bool\" : true,"
                                " \"string\" : \"a string\","
                                " \"null\" : null,"
                                " \"real\" : 3.1415,"
                                " \"vec\" : [1, 2], "
                                " \"nested\" : {"
                                "                  \"foo\" : \"bar\""
                                "               }"
                                "}";

namespace CthunAgent {

TEST_CASE("DataContainer::get", "[data]") {
    DataContainer msg { JSON };

    SECTION("it can get a root value") {
        REQUIRE(msg.get<int>("goo") == 1);
    }

    SECTION("it can get a nested value") {
        REQUIRE(msg.get<int>("foo", "bar") == 2);
    }

    SECTION("it can get a bool value") {
        REQUIRE(msg.get<bool>("bool") == true);
    }

    SECTION("it can get a string value") {
        REQUIRE(msg.get<std::string>("string") == "a string");
    }

    SECTION("it can get a double value") {
        REQUIRE(msg.get<double>("real") == 3.1415);
    }

    SECTION("it can get a vector") {
        std::vector<int> tmp { 1, 2 };
        std::vector<int> result { msg.get<std::vector<int>>("vec") };
        REQUIRE(tmp[0] == result[0]);
        REQUIRE(tmp[1] == result[1]);
    }

    SECTION("it can recurively get a Message object") {
        Message tmp { msg.get<Message>("nested") };
        REQUIRE(tmp.get<std::string>("foo") == "bar");
    }


    SECTION("it should behave correctly given a null value") {
        REQUIRE(msg.get<std::string>("null") == "");
        REQUIRE(msg.get<int>("null") == 0);
        REQUIRE(msg.get<bool>("null") == false);
    }

    SECTION("it returns a null like value when indexing something that doesn't exist") {
        REQUIRE(msg.get<std::string>("invalid") == "");
        REQUIRE(msg.get<int>("goo", "1") == 0);
        REQUIRE(msg.get<bool>("foo", "baz") == false);
    }
}

TEST_CASE("DataContainer::set", "[data]") {
    DataContainer msg {};

    SECTION("it should add a new pair to the root") {
        msg.set<int>(4, "foo");
        REQUIRE(msg.get<int>("foo") == 4);
    }

    SECTION("it allows the creation of a nested structure") {
        msg.set<int>(0, "level1", "level21");
        msg.set<bool>(true, "bool1");
        msg.set<std::string>("a string", "level1", "level22");
        msg.set<std::string>("different string", "level11");
        REQUIRE(msg.get<bool>("bool1") == true);
        REQUIRE(msg.get<int>("level1", "level21") == 0);
        REQUIRE(msg.get<std::string>("level1", "level22") == "a string");
        REQUIRE(msg.get<std::string>("level11") == "different string");
    }

    SECTION("it allows changing a key's value") {
        msg.set<int>(0, "foo");
        REQUIRE(msg.get<int>("foo") == 0);
        msg.set<int>(5, "foo");
        REQUIRE(msg.get<int>("foo") == 5);
    }

    SECTION("it can set a key to a vector") {
        std::vector<std::string> strings { "foo", "bar" };
        msg.set<std::vector<std::string>>(strings, "sv");

        std::vector<int> ints { 4, 2 };
        msg.set<std::vector<int>>(ints, "iv");

        std::vector<bool> bools { true, false };
        msg.set<std::vector<bool>>(bools, "bv");

        std::vector<double> doubles { 0.00, 9.99 };
        msg.set<std::vector<double>>(doubles, "dv");

        REQUIRE(msg.get<std::vector<std::string>>("sv")[0] == "foo");
        REQUIRE(msg.get<std::vector<std::string>>("sv")[1] == "bar");

        REQUIRE(msg.get<std::vector<int>>("iv")[0] == 4);
        REQUIRE(msg.get<std::vector<int>>("iv")[1] == 2);

        REQUIRE(msg.get<std::vector<bool>>("bv")[0] == true);
        REQUIRE(msg.get<std::vector<bool>>("bv")[1] == false);

        REQUIRE(msg.get<std::vector<double>>("dv")[0] == 0.00);
        REQUIRE(msg.get<std::vector<double>>("dv")[1] == 9.99);
    }
}

TEST_CASE("DataContainer::includes", "[data]") {
    DataContainer msg { JSON };
    REQUIRE(msg.includes("foo") == true);
    REQUIRE(msg.includes("foo", "bar") == true);
    REQUIRE(msg.includes("foo", "baz") == false);
}

TEST_CASE("DataContainer::validate", "[data]") {
    valijson::Schema schema;

    valijson::constraints::TypeConstraint json_type_object { valijson::constraints::TypeConstraint::kObject };
    valijson::constraints::TypeConstraint json_type_string { valijson::constraints::TypeConstraint::kString };
    valijson::constraints::TypeConstraint json_type_array { valijson::constraints::TypeConstraint::kArray };
    valijson::constraints::TypeConstraint json_type_int { valijson::constraints::TypeConstraint::kInteger };
    valijson::constraints::TypeConstraint json_type_bool { valijson::constraints::TypeConstraint::kBoolean };
    valijson::constraints::TypeConstraint json_type_null { valijson::constraints::TypeConstraint::kNull };
    valijson::constraints::TypeConstraint json_type_double { valijson::constraints::TypeConstraint::kNumber };

    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties required_properties;

    schema.addConstraint(json_type_object);

    required_properties.insert("goo");
    properties["goo"].addConstraint(json_type_int);

    required_properties.insert("foo");
    properties["foo"].addConstraint(json_type_object);

    required_properties.insert("bool");
    properties["bool"].addConstraint(json_type_bool);

    required_properties.insert("string");
    properties["string"].addConstraint(json_type_string);

    required_properties.insert("null");
    properties["null"].addConstraint(json_type_null);

    required_properties.insert("real");
    properties["real"].addConstraint(json_type_double);

    required_properties.insert("vec");
    properties["vec"].addConstraint(json_type_array);

    required_properties.insert("nested");
    properties["nested"].addConstraint(json_type_object);

    schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                              properties,
                              pattern_properties));

    schema.addConstraint(new valijson::constraints::RequiredConstraint(required_properties));

    DataContainer msg { JSON };
    std::vector<std::string> errors;

    SECTION("it returns true when structure is valid") {
        REQUIRE(msg.validate(schema, errors) == true);
    }

    SECTION("it returns false when the structure is invalid") {
        required_properties.insert("vec");
        properties["vec"].addConstraint(json_type_null);
        schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                             properties,
                             pattern_properties));
        REQUIRE(msg.validate(schema, errors) == false);
        REQUIRE(errors.size() > 0);
    }
}

}  // namespace CthunAgent
