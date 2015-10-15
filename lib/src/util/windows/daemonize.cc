#include <pxp-agent/util/daemonize.hpp>
#include <pxp-agent/util/windows/pid_file.hpp>
#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.daemonize"
#include <leatherman/logging/logging.hpp>

#include <cstdlib>
#include <windows.h>

namespace PXPAgent {
namespace Util {

static std::unique_ptr<PIDFile> _pidf_ptr;

BOOL ctrl_handler(DWORD sig)
{
    switch (sig) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            LOG_DEBUG("Caught signal %1% - removing PID file in %2%",
                      std::to_string(sig), PID_DIR);
            cleanup();
            exit(EXIT_SUCCESS);
        }
        default:
            return FALSE;
    }
}

// Initialize the PID file and set up signal handlers
void daemonize() {
    std::unique_ptr<PIDFile> pidf_ptr { new PIDFile(PID_DIR) };

    if (pidf_ptr->isExecuting()) {
        auto pid = pidf_ptr->read();
        LOG_ERROR("Already running with PID=%1%", pid);
        exit(EXIT_FAILURE);
    }

    try {
        pidf_ptr->lock();
        LOG_DEBUG("Obtained a lock for the PID file; no other pxp-agent "
                  "daemon should be executing");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed get a lock for the PID file: %1%", e.what());
        exit(EXIT_FAILURE);
    }

    auto agent_pid = GetCurrentProcessId();
    pidf_ptr->write(agent_pid);

    if (SetConsoleCtrlHandler(static_cast<PHANDLER_ROUTINE>(ctrl_handler), TRUE)) {
        LOG_DEBUG("Control handler installed");
    } else {
        LOG_ERROR("Could not set control handler");
        exit(EXIT_FAILURE);
    }

    LOG_INFO("Daemonization completed; pxp-agent PID=%1%, PID lock file in '%2%'",
             agent_pid, PID_DIR);

    _pidf_ptr = std::move(pidf_ptr);
}

void cleanup() {
    _pidf_ptr->cleanup();
}

}  // namespace Util
}  // namespace PXPAgent
