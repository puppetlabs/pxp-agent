#include <pxp-agent/util/daemonize.hpp>
#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.daemonize"
#include <leatherman/logging/logging.hpp>
#include <leatherman/windows/system_error.hpp>

#include <cstdlib>
#include <cassert>
#include <windows.h>

namespace PXPAgent {
namespace Util {

namespace lth_win = leatherman::windows;

BOOL WINAPI ctrl_handler(DWORD sig)
{
    switch (sig) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            LOG_INFO("Caught signal %1% - shutting down", std::to_string(sig));
            daemon_cleanup();
            exit(EXIT_SUCCESS);
        }
        default:
            return FALSE;
    }
}

static HANDLE program_lock = INVALID_HANDLE_VALUE;
static constexpr char program_lock_name[] = "com_puppetlabs_pxp-agent";

void daemonize() {
    assert(program_lock == INVALID_HANDLE_VALUE);
    program_lock = CreateMutexA(NULL, TRUE, program_lock_name);
    if (NULL == program_lock) {
        LOG_ERROR("Unable to acquire process lock: %1%", lth_win::system_error());
        exit(EXIT_FAILURE);
    } else if (ERROR_ALREADY_EXISTS == GetLastError()) {
        LOG_ERROR("Already running daemonized");
        exit(EXIT_FAILURE);
    }

    if (SetConsoleCtrlHandler(static_cast<PHANDLER_ROUTINE>(ctrl_handler), TRUE)) {
        LOG_DEBUG("Console control handler installed");
    } else {
        LOG_ERROR("Could not set control handler: %1%", lth_win::system_error());
        daemon_cleanup();
        exit(EXIT_FAILURE);
    }

    LOG_INFO("Daemonization completed; pxp-agent PID=%1%, process lock '%2%'",
             GetCurrentProcessId(), program_lock_name);
}

void daemon_cleanup() {
    // This doesn't currently use RAII because the scope isn't clear, and global static
    // destruction would preclude using logging because Boost.Log also uses global static
    // destruction.
    if (program_lock != INVALID_HANDLE_VALUE && program_lock != NULL) {
        LOG_DEBUG("Removing process lock '%1%'", program_lock_name);
        ReleaseMutex(program_lock);
        CloseHandle(program_lock);
        program_lock = INVALID_HANDLE_VALUE;
    }
}

}  // namespace Util
}  // namespace PXPAgent
