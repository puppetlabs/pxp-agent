#ifndef SRC_AGENT_UTIL_POSIX_PID_FILE_HPP_
#define SRC_AGENT_UTIL_POSIX_PID_FILE_HPP_

#include <sys/types.h>          // pid_t
#include <string>
#include <stdexcept>

namespace PXPAgent {
namespace Util {

class PIDFile {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    static const std::string FILENAME;

    // Throw a PIDFile::Error if dir_path is not a directory.
    // Passing dir_path to enable testing; note that DIR_PATH is
    // hardcoded in Configuration
    PIDFile(const std::string& dir_path);

    // Perform clean up if previously requested by cleanUpWhenDone().
    ~PIDFile();

    // Return true if the PID file stores a PID relative to a
    // running process, false otherwise.
    bool isExecuting();

    // Attempt to lock the PID file.
    // Throw a PIDFile::Error in case it fails to open such file or
    // if it fails to lock it.
    void lock();

    // Write the specified PID number plus a newline.
    // Throw a PIDFile::Error if it fails.
    void write(const pid_t& pid);

    // Read the content of the PID file and convert it to pid_t.
    // Throw a PIDFile::Error in case the file is not readable;
    // is empty; it does not contain an unsigned integer value.
    pid_t read();

    // If previosly locked, unlock the PID file and close its file
    // descriptor.
    // If the PID file exists, remove it.
    void cleanup();

    // Sets the flag that will trigger a cleanup() call by the dtor.
    void cleanupWhenDone();

    // Lock exclusively the specified file descriptor; non blocking.
    static void exclusivelyLockFile(int fd);

    // Unlock the specified file descriptor.
    static void unlockFile(int fd);

    // Checks the PID by sending NULL SIGNAL WITH kill().
    // NB: does not consider recycled PIDs nor zombie processes.
    static bool isProcessExecuting(pid_t pid);

  private:
    std::string dir_path;
    std::string file_path;
    int locked_fd;
    bool cleanup_when_done;
};

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_AGENT_UTIL_POSIX_PID_FILE_HPP_
