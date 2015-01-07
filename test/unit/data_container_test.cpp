#include "test/test.h"
#include "src/data_container.h"

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
using namespace Cthun;
using namespace Agent;

TEST_CASE("Agent::Data::get", "[data]") {
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

    SECTION("it can get a float value") {
        REQUIRE(msg.get<float>("real") == 3.1415f);
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

TEST_CASE("Agent::Data::set", "[data]") {
    DataContainer msg {};

    SECTION("it should add a new pair to the root") {
        msg.set<int>(4, "foo");
        REQUIRE(msg.get<int>("foo") == 4);
    }

    SECTION("it allows the creation of a nested structure") {
        msg.set<int>(0, "level1", "level21");
        msg.set<std::string>("a string", "level1", "level22");
        REQUIRE(msg.get<int>("level1", "level21") == 0);
        REQUIRE(msg.get<std::string>("level1", "level22") == "a string");
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

        std::vector<float> floats { 42.0, 3.1415 };
        msg.set<std::vector<float>>(floats, "fv");

        std::vector<double> doubles { 0.00, 9.99 };
        msg.set<std::vector<double>>(doubles, "dv");

        REQUIRE(msg.get<std::vector<std::string>>("sv")[0] == "foo");
        REQUIRE(msg.get<std::vector<std::string>>("sv")[1] == "bar");

        REQUIRE(msg.get<std::vector<int>>("iv")[0] == 4);
        REQUIRE(msg.get<std::vector<int>>("iv")[1] == 2);

        REQUIRE(msg.get<std::vector<bool>>("bv")[0] == true);
        REQUIRE(msg.get<std::vector<bool>>("bv")[1] == false);

        REQUIRE(msg.get<std::vector<float>>("fv")[0] == 42.0f);
        REQUIRE(msg.get<std::vector<float>>("fv")[1] == 3.1415f);

        REQUIRE(msg.get<std::vector<double>>("dv")[0] == 0.00);
        REQUIRE(msg.get<std::vector<double>>("dv")[1] == 9.99);
    }
}

TEST_CASE("Agent::Data::includes", "[data]") {
    DataContainer msg { JSON };
    REQUIRE(msg.includes("foo") == true);
    REQUIRE(msg.includes("foo", "bar") == true);
    REQUIRE(msg.includes("foo", "baz") == false);


}
