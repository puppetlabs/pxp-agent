#include "test/test.h"

#include "src/common/file_utils.h"
#include "src/common/uuid.h"

#include <iostream>

namespace Cthun {
namespace Common {

TEST_CASE("Common::FileUtils::expandAsDoneByShell", "[common]") {
    SECTION("it should expand the home directory path") {
        REQUIRE(FileUtils::expandAsDoneByShell("~/foo") != "~/foo");
    }

    SECTION("it should not expand the working directory path") {
        REQUIRE(FileUtils::expandAsDoneByShell("./foo") == "./foo");
    }

    std::string home_path { getenv("HOME") };

    SECTION("it should expand ~ to the HOME env var") {
        REQUIRE(FileUtils::expandAsDoneByShell("~") == home_path);
    }

    SECTION("it should expand ~ as the base directory") {
        std::string expected_path { home_path + "/spam" };
        std::string expanded_path { FileUtils::expandAsDoneByShell("~/spam") };
        REQUIRE(expanded_path == expected_path);
    }
}

std::string file_path { FileUtils::expandAsDoneByShell("~/test_" + getUUID()) };

TEST_CASE("Common::FileUtils::fileExists", "[common]") {
    SECTION("it can check that a file does not exist") {
        REQUIRE(FileUtils::fileExists(file_path) == false);
    }
}

TEST_CASE("Common::FileUtils::writeToFile", "[common]") {
    SECTION("it can write to a file, check that it exists, and delete it") {
        REQUIRE(FileUtils::fileExists(file_path) == false);
        FileUtils::writeToFile("test\n", file_path);
        REQUIRE(FileUtils::fileExists(file_path) == true);
        FileUtils::removeFile(file_path);
        REQUIRE(FileUtils::fileExists(file_path) == false);
    }
}

}  // namespace Common
}  // namespace Cthun
