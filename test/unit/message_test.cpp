#include "test/test.h"

#include "src/message.h"

static const std::string JSON = "{\"foo\" : {\"bar\" : 2},"
                                " \"goo\" : 1,"
                                " \"bool\" : true,"
                                " \"string\" : \"a string\","
                                " \"null\" : null }";

TEST_CASE("Agent::Message::get", "[message]") {
    Agent::Message msg { JSON };

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

    SECTION("it should behave correctly given a null value") {
        REQUIRE(msg.get<std::string>("null") == "");
        REQUIRE(msg.get<int>("null") == 0);
        REQUIRE(msg.get<bool>("null") == false);
    }

    SECTION("it throws when you don't index correctly") {
        REQUIRE_THROWS_AS(msg.get<int>("invalid"), Agent::message_index_error);
        REQUIRE_THROWS_AS(msg.get<int>("goo", "1"), Agent::message_index_error);
        REQUIRE_THROWS_AS(msg.get<int>("foo", "baz"), Agent::message_index_error);
    }
}

TEST_CASE("Agent::Message::set", "[message]") {
    Agent::Message msg {};

    SECTION("it should add a new pair to the root") {
        msg.set<int>(4, "foo");
        REQUIRE(msg.get<int>("foo") == 4);
    }

    SECTION("it allows the creation of a nested structure") {
        msg.set<int>(0, "level1", "level2");
        REQUIRE(msg.get<int>("level1", "level2") == 0);
    }

    SECTION("it allows changing a key's value") {
        msg.set<int>(5, "foo");
        REQUIRE(msg.get<int>("foo") == 5);
    }
}
