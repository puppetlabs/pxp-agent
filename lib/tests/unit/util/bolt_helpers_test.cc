#include <pxp-agent/util/bolt_helpers.hpp>

#include <catch.hpp>

using namespace PXPAgent;
using namespace Util;

TEST_CASE("splitArguments", "[util]") {
    SECTION("double-quoted executable is separated from the arguments") {
        auto args_vector = splitArguments("\"C:\\Program Files (x86)\\Executable with Spaces.exe\" <arg1> <arg2>");
        REQUIRE(args_vector.at(0) == "C:\\Program Files (x86)\\Executable with Spaces.exe");
        REQUIRE(args_vector.at(1) == "<arg1>");
        REQUIRE(args_vector.at(2) == "<arg2>");
    }
    SECTION("single-quoted executable is separated from the arguments") {
        auto args_vector = splitArguments("\'C:\\Program Files (x86)\\Executable with Spaces.exe\' <arg1> <arg2>");
        REQUIRE(args_vector.at(0) == "C:\\Program Files (x86)\\Executable with Spaces.exe");
        REQUIRE(args_vector.at(1) == "<arg1>");
        REQUIRE(args_vector.at(2) == "<arg2>");
    }
    SECTION("double-quoted args are parsed correctly") {
        auto args_vector = splitArguments("rm -f \"Untitled 1.rtf\" \"Untitled 2.rtf\"");
        REQUIRE(args_vector.at(0) == "rm");
        REQUIRE(args_vector.at(1) == "-f");
        REQUIRE(args_vector.at(2) == "Untitled 1.rtf");
        REQUIRE(args_vector.at(3) == "Untitled 2.rtf");
    }
    SECTION("single-quoted args are parsed correctly") {
        auto args_vector = splitArguments("rm -f \'Untitled 1.rtf\' \'Untitled 2.rtf\'");
        REQUIRE(args_vector.at(0) == "rm");
        REQUIRE(args_vector.at(1) == "-f");
        REQUIRE(args_vector.at(2) == "Untitled 1.rtf");
        REQUIRE(args_vector.at(3) == "Untitled 2.rtf");
    }
}
