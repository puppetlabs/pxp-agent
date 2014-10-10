#include "test/test.h"

#include "src/common/file_utils.h"
#include "src/common/uuid.h"

#include <iostream>

namespace Cthun {
namespace Common {

class FileUtilsTest : public ::testing::Test {};

// Can expand a file path
TEST(FileUtilsTest, ExpandAsDoneByShell) {
    EXPECT_NE("~/foo", FileUtils::expandAsDoneByShell("~/foo"));
    EXPECT_EQ("./foo", FileUtils::expandAsDoneByShell("./foo"));
}

// Can expand the HOME env var
TEST(FileUtilsTest, ExpandAsDoneByShell_bis) {
    std::string home_path { getenv("HOME") };
    std::string expanded_home { FileUtils::expandAsDoneByShell("~") };

    EXPECT_EQ(home_path, expanded_home);

    expanded_home = FileUtils::expandAsDoneByShell("$HOME");

    EXPECT_EQ(home_path, expanded_home);

    std::string expected_path { home_path + "/spam" };
    std::string expanded_path { FileUtils::expandAsDoneByShell("~/spam") };

    EXPECT_EQ(expected_path, expanded_path);
}

// Can check that a file does not exist
TEST(FileUtilsTest, FileExists_negative) {
    EXPECT_FALSE(FileUtils::fileExists("./asdklfj" + getUUID()));
}

// Can check that a file exists; also can write and remove files
TEST(FileUtilsTest, FileExists_WriteAndRemove) {
    std::string file_path {
        FileUtils::expandAsDoneByShell("~/test_" + getUUID()) };
    EXPECT_FALSE(FileUtils::fileExists(file_path));
    FileUtils::writeToFile("test\n", file_path);
    EXPECT_TRUE(FileUtils::fileExists(file_path));
    FileUtils::removeFile(file_path);
    EXPECT_FALSE(FileUtils::fileExists(file_path));
}

}  // namespace Common
}  // namespace Cthun
