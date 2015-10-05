#include <pxp-agent/util/posix/pid_file.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.posix.pid_file"
#include <leatherman/logging/logging.hpp>

#include <cstdio>           // remove()
#include <cassert>

#include <sys/file.h>       // open()
#include <sys/stat.h>
#include <fcntl.h>          // open() flags
#include <signal.h>         // kill()
#include <unistd.h>         // getpid()

namespace PXPAgent {
namespace Util {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;

const int FD_PUN { -1 };

const std::string PIDFile::FILENAME { "pxp-agent.pid" };

PIDFile::PIDFile(const std::string& dir_path_)
        : dir_path { dir_path_ },
          file_path { dir_path + "/" + FILENAME },
          locked_fd { FD_PUN },
          cleanup_when_done { false } {
    if (fs::exists(dir_path)) {
        if (!fs::is_directory(dir_path)) {
            throw PIDFile::Error { "the PID directory '" + dir_path
                                   + "' is not a directory" };
        }
    } else {
        LOG_INFO("Creating PID directory '%1%'", dir_path);
        try {
            fs::create_directories(dir_path);
        } catch (const fs::filesystem_error& e) {
            throw PIDFile::Error { e.what() };
        }
    }
}

PIDFile::~PIDFile() {
    if (cleanup_when_done) {
        try {
            cleanup();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to clean up PID file %1%: %2%", file_path, e.what());
        }
    }
}

bool PIDFile::isExecuting() {
    try {
        auto pid = PIDFile::read();
        return isProcessExecuting(pid);
    } catch (const PIDFile::Error& e) {
        LOG_DEBUG("Failed to read PID file %1%: %2%", file_path, e.what());
    }

    return false;
}

void PIDFile::lock() {
    locked_fd = open(file_path.data(), O_RDWR | O_CREAT, 0640);

    if (locked_fd == -1) {
        std::string msg { "failed to open "  };
        throw PIDFile::Error { msg + file_path };
    }

    PIDFile::exclusivelyLockFile(locked_fd);
}

void PIDFile::write(const pid_t& pid) {
    try {
        lth_file::atomic_write_to_file(std::to_string(pid) + "\n", file_path);
    } catch (const std::exception& e) {
        std::string msg { "failed to write " };
        throw PIDFile::Error { msg + file_path + ": " + e.what() };
    }
}

pid_t PIDFile::read() {
    if (!fs::exists(file_path)) {
        throw PIDFile::Error { "PID file does not exist" };
    }

    if (!fs::is_regular_file(file_path)) {
        throw PIDFile::Error { "PID file is not a regular file" };
    }

    if (!lth_file::file_readable(file_path)) {
        throw PIDFile::Error { "cannot read PID file" };
    }

    auto pid_txt = lth_file::read(file_path);

    if (pid_txt.empty()) {
        throw PIDFile::Error { "PID file is empty" };
    }

    try {
        auto pid = std::stoi(pid_txt);
        return static_cast<pid_t>(pid);
    } catch (const std::invalid_argument& e) {
        throw PIDFile::Error { "invalid value stored in PID file" };
    }
}

void PIDFile::cleanup() {
    if (locked_fd != FD_PUN) {
        LOG_DEBUG("Unlocking PID file %1% and closing its open file descriptor",
                  file_path);
        PIDFile::unlockFile(locked_fd);
        close(locked_fd);
    }

    if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
        if (lth_file::file_readable(file_path)) {
            if (std::remove(file_path.data())) {
                LOG_ERROR("Failed to remove PID file %1%", file_path);
            } else {
                LOG_DEBUG("Removed PID file %1%", file_path);
            }
        } else {
            LOG_DEBUG("Cannot access PID file %1%", file_path);
        }
    } else {
        LOG_DEBUG("PID file %1% does not exist", file_path);
    }
}

void PIDFile::cleanupWhenDone() {
    cleanup_when_done = true;
}

// Static function members

void PIDFile::exclusivelyLockFile(int fd) {
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        std::string msg { "failed to lock: "};

        if (errno == EWOULDBLOCK) {
            msg += "already locked by another process";
        } else {
            msg += "unexpected error with errno=" + std::to_string(errno);
        }

        throw PIDFile::Error { msg };
    }
}

void PIDFile::unlockFile(int fd) {
    if (flock(fd, LOCK_UN) == -1) {
        std::string msg { "failed to lock open file descriptor "};
        throw PIDFile::Error { msg + std::to_string(fd)
                               + " with errno=" + std::to_string(errno) };
    }
}

bool PIDFile::isProcessExecuting(pid_t pid) {
    if (kill(pid, 0)) {
        switch (errno) {
            case ESRCH:
                return false;
            case EPERM:
                // Process exists, but we can't signal to it
                return true;
            default:
                // Unexpected
                return false;
        }
    }

    return true;
}

}  // namespace Util
}  // namespace PXPAgent
