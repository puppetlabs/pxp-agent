#include <pxp-agent/util/process.hpp>

#include <signal.h>
#include <errno.h>
#include <unistd.h>         // getpid()

namespace PXPAgent {
namespace Util {

// Checks the PID by sending NULL SIGNAL WITH kill().
// NB: does not consider recycled PIDs nor zombie processes.
bool processExists(int pid) {
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

int getPid() {
    return getpid();
}

}  // namespace Util
}  // namespace PXPAgent
