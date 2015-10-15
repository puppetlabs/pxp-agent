#include <pxp-agent/util/windows/pid_file.hpp>

#include <boost/filesystem.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/windows/system_error.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.windows.pid_file"
#include <leatherman/logging/logging.hpp>

#include <windows.h>

namespace PXPAgent {
namespace Util {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace lth_win = leatherman::windows;

const std::string PIDFile::FILENAME { "pxp-agent.pid" };

PIDFile::PIDFile(const std::string& dir_path_)
        : dir_path { dir_path_ },
          file_path { dir_path / FILENAME },
          pidfile_fd {} {
    if (fs::exists(dir_path)) {
        if (!fs::is_directory(dir_path)) {
            throw PIDFile::Error { "the PID directory '" + dir_path.string()
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

    pidfile_fd = CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, nullptr);

    if (pidfile_fd == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open PID file '%1%'; error=%2%", file_path, GetLastError());
        std::string msg { "failed to open PID file" };
        throw PIDFile::Error { msg };
    }
}

PIDFile::~PIDFile() {
    if (pidfile_fd != INVALID_HANDLE_VALUE) {
        cleanup(false);
    }
}

void PIDFile::cleanup(bool log) {
    if (log) LOG_DEBUG("Unlocking PID file %1% and closing its open file descriptor", file_path);
    if (!UnlockFile(pidfile_fd, 0, 0, sizeof(DWORD), 0)) {
        if (log) LOG_ERROR("Failed to unlock PID file %1%: %2%", file_path, lth_win::system_error());
    }
    CloseHandle(pidfile_fd);
    pidfile_fd = INVALID_HANDLE_VALUE;

    boost::system::error_code ec;
    if (fs::exists(file_path, ec) && fs::is_regular_file(file_path, ec)) {
        if (DeleteFileW(file_path.c_str())) {
            if (log) LOG_DEBUG("Removed PID file %1%", file_path);
        } else {
            if (log) LOG_ERROR("Failed to remove PID file %1%", file_path);
        }
    } else {
        if (log) LOG_DEBUG("PID file %1% does not exist", file_path);
    }

    if (ec) {
        if (log) LOG_ERROR("Failed to clean up PID file %1%: %2%", file_path, ec.message());
    }
}

bool PIDFile::isExecuting() {
    try {
        auto pid = PIDFile::read();
        auto process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(process, &exitCode)) {
            LOG_DEBUG("Failed to retrieve status of process %1%: %2%", pid, lth_win::system_error());
            return false;
        }
        return exitCode == STILL_ACTIVE;
    } catch (const PIDFile::Error& e) {
        LOG_DEBUG("Couldn't retrieve PID from file %1%: %2%", file_path, e.what());
    }

    return false;
}

void PIDFile::lock() {
    if (!LockFile(pidfile_fd, 0, 0, sizeof(DWORD), 0)) {
        throw PIDFile::Error { "unable to lock: " + lth_win::system_error() };
    }
}

void PIDFile::write(DWORD pid) {
    // When using Windows file locks, the OS read/write operations also need to be used.
    auto pid_str = std::to_string(pid);
    DWORD written = 0;
    if (!WriteFile(pidfile_fd, pid_str.c_str(), pid_str.size(), &written, nullptr) || written != pid_str.size()) {
        throw PIDFile::Error { "failed to write " + file_path.string() + ": " + lth_win::system_error() };
    }
}

DWORD PIDFile::read() {
    if (!fs::exists(file_path)) {
        throw PIDFile::Error { "PID file does not exist" };
    }

    if (!fs::is_regular_file(file_path)) {
        throw PIDFile::Error { "PID file is not a regular file" };
    }

    // When using Windows file locks, the OS read/write operations also need to be used.
    DWORD numread = 0;
    std::string pid_txt(12, '\0');
    if (!ReadFile(pidfile_fd, &pid_txt[0], pid_txt.size(), &numread, nullptr)) {
        throw PIDFile::Error { "cannot read PID file:" + lth_win::system_error() };
    }
    pid_txt.resize(numread);

    if (pid_txt.empty()) {
        throw PIDFile::Error { "PID file is empty" };
    }

    try {
        auto pid = std::stoi(pid_txt);
        return static_cast<DWORD>(pid);
    } catch (const std::exception& e) {
        // Could be not a number or a number that couldn't possibly be a PID
        throw PIDFile::Error { "invalid value stored in PID file" };
    }
}

}  // namespace Util
}  // namespace PXPAgent
