#include "root_path.hpp"

#include <pxp-agent/util/posix/pid_file.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#include <catch.hpp>

#include <sys/file.h>    // open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>       // open() flags
#include <unistd.h>      // close()

namespace PXPAgent {
namespace Util {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;

const std::string LOCK_PATH { std::string { PXP_AGENT_ROOT_PATH }
                              + "/lib/tests/resources/test_spool/tmp.lock" };
const std::string PID_DIR { std::string { PXP_AGENT_ROOT_PATH }
                            + "/lib/tests/resources/config" };
const std::string TMP_DIR { std::string { PXP_AGENT_ROOT_PATH }
                            + "/lib/tests/resources/test_spool/tmp_pid" };

void initializeTmpPIDFile(const std::string dir, const std::string& txt) {
    if (!fs::exists(dir) && !fs::create_directories(dir)) {
        FAIL("failed to create tmp_pid directory");
    }

    std::string pid_file_path { dir + "/" + PIDFile::FILENAME };

    try {
        lth_file::atomic_write_to_file(txt + "\n", pid_file_path);
    } catch (const std::exception&) {
        FAIL("failed to write temporary pid file");
        fs::remove_all(dir);
    }
}

TEST_CASE("PIDFile ctor", "[util]") {
    SECTION("can instantiate") {
        REQUIRE_NOTHROW(PIDFile { PID_DIR });
    }

    SECTION("can instantiate if the directory does not exist") {
        REQUIRE_NOTHROW(PIDFile { PID_DIR + "/foo/bar" });
    }

    SECTION("cannot instantiate if the directory is a regular file") {
        REQUIRE_THROWS_AS(PIDFile { std::string { PXP_AGENT_ROOT_PATH }
                                    + "/README.md" },
                          PIDFile::Error);
    }
}

TEST_CASE("PIDFile::isExecuting") {
    SECTION("the file contains the PID of an executing process") {
        auto pid = getpid();
        std::string pid_txt { std::to_string(pid) };
        initializeTmpPIDFile(TMP_DIR, pid_txt);
        PIDFile p_f { TMP_DIR };

        REQUIRE(p_f.isExecuting());
    }

    SECTION("returns false if the file does not exist") {
        fs::remove_all(TMP_DIR);
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    SECTION("returns false if the file is empty") {
        initializeTmpPIDFile(TMP_DIR, "");
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    SECTION("returns false if the file contains an invalid integer string") {
        initializeTmpPIDFile(TMP_DIR, "spam");
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::read", "[util]") {
    SECTION("can successfully read") {
        pid_t pid { 12340987 };
        initializeTmpPIDFile(TMP_DIR, std::to_string(pid));
        PIDFile p_f { TMP_DIR };

        REQUIRE(p_f.read() == pid);
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::write", "[util]") {
    SECTION("successfully writes into the PID file") {
        fs::remove_all(TMP_DIR);
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };
        pid_t pid { 424242 };
        p_f.write(pid);
        auto read_pid = p_f.read();

        REQUIRE(pid == read_pid);
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::cleanup", "[util]") {
    SECTION("successfully removes the PID file") {
        fs::remove_all(TMP_DIR);
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };
        pid_t pid { 353535 };
        p_f.write(pid);
        std::string pid_file_path { TMP_DIR + "/" + PIDFile::FILENAME };

        REQUIRE(lth_file::file_readable(pid_file_path));

        p_f.cleanup();

        REQUIRE_FALSE(lth_file::file_readable(pid_file_path));
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::exclusivelyLockFile", "[util]") {
    SECTION("it can lock a file") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);

        REQUIRE_NOTHROW(PIDFile::exclusivelyLockFile(fd));
        close(fd);    }

    SECTION("cannot lock a file that is locked by a different file descriptor") {
        auto first_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (first_fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        auto second_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (second_fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        PIDFile::exclusivelyLockFile(first_fd);

        REQUIRE_THROWS_AS(PIDFile::exclusivelyLockFile(second_fd),
                          PIDFile::Error);

        close(first_fd);
        close(second_fd);
    }

    SECTION("locking is idempotent, i.e. we can lock the same open fd "
            "multiple times") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        PIDFile::exclusivelyLockFile(fd);

        REQUIRE_NOTHROW(PIDFile::exclusivelyLockFile(fd));
        REQUIRE_NOTHROW(PIDFile::exclusivelyLockFile(fd));
        close(fd);
    }
}

TEST_CASE("PIDFile::unlockFile", "[util]") {
    SECTION("it can successfully unlock a locked file") {
        auto first_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (first_fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        auto second_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (second_fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        PIDFile::exclusivelyLockFile(first_fd);

        // Can't lock; it's already locked
        REQUIRE_THROWS_AS(PIDFile::exclusivelyLockFile(second_fd),
                          PIDFile::Error);

        // Unlocking a locked file is ok
        REQUIRE_NOTHROW(PIDFile::unlockFile(first_fd));

        // Unlocking works
        REQUIRE_NOTHROW(PIDFile::exclusivelyLockFile(second_fd));

        close(first_fd);
        close(second_fd);
    }

    SECTION("unlocking is idempotent, i.e. we can unlock an unlocked file") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);

        REQUIRE_NOTHROW(PIDFile::unlockFile(fd));
        REQUIRE_NOTHROW(PIDFile::unlockFile(fd));
        close(fd);
    }
}

TEST_CASE("PIDFile::isProcessExecuting", "[util]") {
    SECTION("this process is executing") {
        REQUIRE(PIDFile::isProcessExecuting(getpid()));
    }
}

}  // namespace Util
}  // namespace PXPAgent
