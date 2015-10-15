#ifndef SRC_AGENT_UTIL_WINDOWS_PID_FILE_HPP_
#define SRC_AGENT_UTIL_WINDOWS_PID_FILE_HPP_

#include <string>
#include <stdexcept>
#include <boost/filesystem/path.hpp>
using HANDLE = void *;
using DWORD = unsigned long;

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

    // Perform clean up unless already done.
    ~PIDFile();

    // Disable copies, and provide move semantics.
    PIDFile(PIDFile const&) = delete;
    PIDFile& operator=(PIDFile const&) = delete;
    PIDFile(PIDFile &&) = default;
    PIDFile& operator=(PIDFile &&) = default;

    // Perform clean up; use logging if log is true
    // logging is disabled in the destructor, as we can't guarantee
    // it'll be destroyed before Boost.Log is shut down
    void cleanup(bool log = true);

    // Return true if the PID file stores a PID relative to a
    // running process, false otherwise.
    bool isExecuting();

    // Try to acquire a lock for the PID file.
    // Throw a PIDFile::Error in case it fails to lock.
    // Throw a PIDFile::Error in case of system call error.
    void lock();

    // Write the specified PID number plus a newline.
    // Throw a PIDFile::Error if it fails.
    void write(DWORD pid);

    // Read the content of the PID file.
    // Throw a PIDFile::Error in case the file is not readable;
    // is empty; it does not contain an unsigned integer value.
    DWORD read();

  private:
    boost::filesystem::path dir_path;
    boost::filesystem::path file_path;
    HANDLE pidfile_fd;
};

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_AGENT_UTIL_WINDOWS_PID_FILE_HPP_
