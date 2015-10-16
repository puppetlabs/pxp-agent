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

    // Create the PID file directory if necessary.
    // Open the PID file.
    // Throw a PIDFile::Error if dir_path is not a directory or in
    // case of a failure when opening the PID file.
    // NB: we pass dir_path to enable testing; DIR_PATH is hardcoded
    // in Configuration
    PIDFile(const std::string& dir_path);

    // Perform clean up if previously requested by cleanUpWhenDone().
    ~PIDFile();

    // Disable copies, and provide move semantics.
    PIDFile(PIDFile const&) = delete;
    PIDFile& operator=(PIDFile const&) = delete;
    PIDFile(PIDFile &&) = default;
    PIDFile& operator=(PIDFile &&) = default;

    // Return true if the PID file stores a PID relative to a
    // running process, false otherwise.
    bool isExecuting();

    // Try to acquire a read lock for the PID file.
    // Throw a PIDFile::Error in case it fails to lock or, in case
    // 'blocking' is flagged, wait until the lock can be performed.
    // Throw a PIDFile::Error in case of system call error.
    void lockRead(bool blocking = false);

    // Try to acquire a write lock for the PID file.
    // Throw a PIDFile::Error in case it fails to lock or, in case
    // 'blocking' is flagged, wait until the lock can be performed.
    // Throw a PIDFile::Error in case of system call error.
    void lockWrite(bool blocking = false);

    // Return true if it is possible to acquire a read lock for the
    // PID file, false otherwise.
    // Throw a PIDFile::Error in case it fails to perform the check.
    bool canLockRead();

    // Return true if it is possible to acquire a write lock for the
    // PID file, false otherwise.
    // Throw a PIDFile::Error in case it fails to perform the check.
    bool canLockWrite();

    // Write the specified PID number plus a newline.
    // Throw a PIDFile::Error if it fails.
    void write(const pid_t& pid);

    // Read the content of the PID file and convert it to pid_t.
    // Throw a PIDFile::Error in case the file is not readable;
    // is empty; it does not contain an unsigned integer value.
    pid_t read();

    // If previously locked, unlock the PID file and close its file
    // descriptor.
    // If the PID file exists, remove it.
    void cleanup();

    // Sets the flag that will trigger a cleanup() call by the dtor.
    void cleanupWhenDone();

    // Checks the PID by sending NULL SIGNAL WITH kill().
    // NB: does not consider recycled PIDs nor zombie processes.
    static bool isProcessExecuting(pid_t pid);

    // Lock the file related to the specified file descriptor with the
    // specified lock type (F_RDLCK or F_WRLCK).
    // NB: the created lock will be specific to this process and the
    // inode - it will not be inherited by child processes; the
    // function relies on the fcntl system call.
    // Non blocking call; will throw a PIDFile::Error in case it fails
    // to lock immediately.
    static void lockFile(int fd, int lock_type);

    // Lock the file related to the specified file descriptor with the
    // specified lock type (F_RDLCK or F_WRLCK).
    // NB: the created lock will be specific to this process and the
    // inode - it will not be inherited by child processes; the
    // function relies on the fcntl system call.
    // Blocking call; will throw a PIDFile::Error in case it fails
    // to perform the fcntl system call or in case an interrupt
    // signal is caught.
    static void lockFileBlocking(int fd, int lock_type);

    // Release any lock associated with this process associated with
    // the file related to specified file descriptor.
    static void unlockFile(int fd);

    // Return true if it's possible to acquire the specified lock type
    // (F_RDLCK or F_WRLCK) on the specified file descriptor, false
    // otherwise.
    // Throw a PIDFile::Error in case it fails to perform the check.
    static bool canLockFile(int fd, int lock_type);

  private:
    std::string dir_path;
    std::string file_path;
    int pidfile_fd;
    bool cleanup_when_done;
};

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_AGENT_UTIL_POSIX_PID_FILE_HPP_
