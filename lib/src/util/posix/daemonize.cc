#include <pxp-agent/util/posix/daemonize.hpp>
#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.posix.daemonize"
#include <leatherman/logging/logging.hpp>

#include <sys/types.h>
#include <stdlib.h>         // exit()
#include <unistd.h>         // _exit(), getpid(), fork(), setsid(), chdir()
#include <sys/wait.h>       // waitpid()
#include <sys/stat.h>       // umask()
#include <signal.h>
#include <stdio.h>

#include <memory>
#include <vector>

namespace PXPAgent {
namespace Util {

const mode_t UMASK_FLAGS { 002 };
const std::string DEFAULT_DAEMON_WORKING_DIR = "/";

static void sigHandler(int sig) {
    // NOTE(ale): if issued by the init process, between SIGTERM and
    // the successive SIGKILL there are only 5 s - must be fast

    if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT) {
        auto pid_dir = Configuration::Instance().get<std::string>("pid-dir");
        LOG_DEBUG("Caught signal %1% - removing PID file in %2%",
                  std::to_string(sig), pid_dir);
        PIDFile pidf { pid_dir };
        pidf.cleanup();
        exit(EXIT_SUCCESS);
    }
}

std::unique_ptr<PIDFile> daemonize() {
    // Check if we're already a daemon
    if (getppid() == 1) {
        // Parent is init process (PID 1)
        LOG_INFO("Already a daemon with PID=%1%", std::to_string(getpid()));
        return nullptr;
    }

    // Lock the PID file

    auto pid_dir = Configuration::Instance().get<std::string>("pid-dir");
    std::unique_ptr<PIDFile> pidf_ptr { new PIDFile(pid_dir) };

    if (pidf_ptr->isExecuting()) {
        auto pid = pidf_ptr->read();
        LOG_ERROR("Already running with PID=%1%", pid);
        exit(EXIT_FAILURE);
    }

    try {
        pidf_ptr->lock();
        LOG_DEBUG("Locked the PID file; no other daemon should be executing");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed to lock the PID file: %1%", e.what());
        exit(EXIT_FAILURE);
    }

    auto removeLockAndExit = [&pidf_ptr] (int status) {
                                 pidf_ptr->cleanupWhenDone();
                                 exit(status);
                             };

    // First fork - run in background

    auto first_child_pid = fork();

    switch (first_child_pid) {
        case -1:
            LOG_ERROR("Failed to perform the first fork; errno=%1%",
                      std::to_string(errno));
            removeLockAndExit(EXIT_FAILURE);
        case 0:
            // CHILD - will fork and exit soon
            LOG_DEBUG("First child spawned, with PID=%1%",
                      std::to_string(getpid()));
            break;
        default:
            // PARENT - wait for the child, to avoid a zombie process
            waitpid(first_child_pid, nullptr, 0);
            // Exit with _exit(); note that after a fork(), only one
            // of parent and child should terminate with exit()
            _exit(EXIT_SUCCESS);
    }

    // Create a new group session and become leader in order to
    // disassociate from a possible controlling terminal later

    if (setsid() == -1) {
        LOG_ERROR("Failed to create new session for first child process; "
                  "errno=%1%", std::to_string(errno));
        removeLockAndExit(EXIT_FAILURE);
    }

    // Second fork - the child won't be a session leader and won't be
    // associated with any controlling terminal

    auto second_child_pid = fork();

    switch (second_child_pid) {
        case -1:
            LOG_ERROR("Failed to perform the second fork; errno=%1%",
                      std::to_string(errno));
            removeLockAndExit(EXIT_FAILURE);
        case 0:
            // CHILD - will be the pxp-agent!
            break;
        default:
            // PARENT - just exit
            _exit(EXIT_SUCCESS);
    }

    auto agent_pid = getpid();
    LOG_DEBUG("Second child spawned, with PID=%1%", agent_pid);

    // Write PID to file
    pidf_ptr->write(agent_pid);

    // Set umask
    umask(UMASK_FLAGS);

    // Change work directory
    if (chdir(DEFAULT_DAEMON_WORKING_DIR.data())) {
        LOG_ERROR("Failed to change work directory to '%1%'; errno=%2%",
                  DEFAULT_DAEMON_WORKING_DIR, std::to_string(errno));
        removeLockAndExit(EXIT_FAILURE);
    } else {
        LOG_DEBUG("Changed working directory to '%1%'", DEFAULT_DAEMON_WORKING_DIR);
    }

    // Change signal dispositions

    for (auto s : std::vector<int> { SIGINT, SIGTERM, SIGQUIT }) {
        if (signal(s, sigHandler) == SIG_ERR) {
            LOG_ERROR("Failed to set signal handler for sig %1%", s);
            removeLockAndExit(EXIT_FAILURE);
        }
    }

    signal(SIGCHLD, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    // TODO(ale): implement reloading after SIGHUP
    signal(SIGHUP, SIG_IGN);

    // Redirect standard files; we always use boost::log anyway
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    LOG_INFO("Daemonization completed; pxp-agent PID=%1%, PID lock file in '%2%'",
             agent_pid, pid_dir);

    // Set PIDFile dtor to clean itself up and return pointer for RAII
    pidf_ptr->cleanupWhenDone();

    return std::move(pidf_ptr);
}

}  // namespace Util
}  // namespace PXPAgent
