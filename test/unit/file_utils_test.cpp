#include "test/test.h"

#include "src/file_utils.h"
#include "src/uuid.h"

#include <iostream>

namespace CthunAgent {

TEST_CASE("FileUtils::shellExpand", "[utils]") {
    SECTION("it should expand the home directory path") {
        REQUIRE(FileUtils::shellExpand("~/foo") != "~/foo");
    }

    SECTION("it should not expand the working directory path") {
        REQUIRE(FileUtils::shellExpand("./foo") == "./foo");
    }

    std::string home_path { getenv("HOME") };

    SECTION("it should expand ~ to the HOME env var") {
        REQUIRE(FileUtils::shellExpand("~") == home_path);
    }

    SECTION("it should expand ~ as the base directory") {
        std::string expected_path { home_path + "/spam" };
        std::string expanded_path { FileUtils::shellExpand("~/spam") };
        REQUIRE(expanded_path == expected_path);
    }
}

static const auto home_path = FileUtils::shellExpand("~");
static const auto file_path =
    FileUtils::shellExpand("~/test_file_" + UUID::getUUID());
static const auto dir_path =
    FileUtils::shellExpand("~/test_dir_" + UUID::getUUID());

TEST_CASE("FileUtils::fileExists", "[utils]") {
    SECTION("it can check that a file does not exist") {
        REQUIRE(FileUtils::fileExists(file_path) == false);
    }

    SECTION("it can check that a directory exists") {
        REQUIRE(FileUtils::fileExists(home_path) == true);
    }
}

TEST_CASE("FileUtils::writeToFile", "[utils]") {
    SECTION("it can write to a regular file, ensure it exists, and delete it") {
        REQUIRE(FileUtils::fileExists(file_path) == false);
        FileUtils::writeToFile("test\n", file_path);
        REQUIRE(FileUtils::fileExists(file_path) == true);
        FileUtils::removeFile(file_path);
        REQUIRE(FileUtils::fileExists(file_path) == false);
    }
}

TEST_CASE("FileUtils::createDirectory", "[utils]") {
    SECTION("it can create and remove an empty directory") {
        REQUIRE(FileUtils::fileExists(dir_path) == false);
        FileUtils::createDirectory(dir_path);
        REQUIRE(FileUtils::fileExists(dir_path) == true);
        FileUtils::removeFile(dir_path);
        REQUIRE(FileUtils::fileExists(dir_path) == false);
    }
}

}  // namespace CthunAgent
