#include <pxp-agent/util/process.hpp>

#include <leatherman/windows/windows.hpp>
#include <leatherman/windows/system_error.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.windows.process"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Util {

namespace lth_win = leatherman::windows;

bool processExists(int pid) {
    auto p_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (p_handle) {
        CloseHandle(p_handle);
        return true;
    } else {
        LOG_TRACE("OpenProcess failure while trying to determine the state "
                  "of PID %1%: %2%", pid, lth_win::system_error());
    }
    return false;
}

}  // namespace Util
}  // namespace PXPAgent
