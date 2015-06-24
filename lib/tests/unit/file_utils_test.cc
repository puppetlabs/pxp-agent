#include <catch.hpp>

#include <cthun-agent/file_utils.hpp>
#include <cthun-agent/uuid.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

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
        REQUIRE_FALSE(FileUtils::fileExists(file_path));
    }

    SECTION("it can check that a directory exists") {
        REQUIRE(FileUtils::fileExists(home_path));
    }
}

TEST_CASE("FileUtils::writeToFile", "[utils]") {
    SECTION("it can write to a regular file, ensure it exists, and delete it") {
        REQUIRE_FALSE(FileUtils::fileExists(file_path));
        FileUtils::writeToFile("test\n", file_path);
        REQUIRE(FileUtils::fileExists(file_path));
        FileUtils::removeFile(file_path);
        REQUIRE_FALSE(FileUtils::fileExists(file_path));
    }
}

TEST_CASE("FileUtils::removeFile", "[utils]") {
    SECTION("it can check that symlink exists and delete it") {
        auto symlink_name = UUID::getUUID();
        std::string symlink_path { "/tmp/" + symlink_name };
        boost::filesystem::path to { FileUtils::shellExpand("~") };
        boost::filesystem::path symlink { symlink_path };
        try {
            boost::filesystem::create_symlink(to, symlink);
            REQUIRE(FileUtils::fileExists(symlink_path));
            FileUtils::removeFile(symlink_path);
            REQUIRE_FALSE(FileUtils::fileExists(symlink_path));
        } catch (boost::filesystem::filesystem_error) {
            FAIL("Failed to create the symlink");
        }
    }
}

TEST_CASE("FileUtils::createDirectory", "[utils]") {
    SECTION("it can create and remove an empty directory") {
        REQUIRE_FALSE(FileUtils::fileExists(dir_path));
        FileUtils::createDirectory(dir_path);
        REQUIRE(FileUtils::fileExists(dir_path));
        FileUtils::removeFile(dir_path);
        REQUIRE_FALSE(FileUtils::fileExists(dir_path));
    }
}

// TODO(ale): test writeToFile/readFileAsString for race conditions

}  // namespace CthunAgent
