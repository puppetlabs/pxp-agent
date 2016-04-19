#include <pxp-agent/util/daemonize.hpp>
#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.daemonize"
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
    auto pidfile = Configuration::Instance().get<std::string>("pidfile");
    LOG_INFO("Caught signal %1% - removing PID file '%2%'",
             std::to_string(sig), pidfile);
    PIDFile pidf { pidfile };
    pidf.cleanup();
    exit(EXIT_SUCCESS);
}

static void dumbSigHandler(int sig) {}

std::unique_ptr<PIDFile> daemonize() {
    // Check if we're already a daemon
    if (getppid() == 1) {
        // Parent is init process (PID 1)
        LOG_INFO("Already a daemon with PID=%1%", std::to_string(getpid()));
        return nullptr;
    }

    // Set umask; child processes will inherit
    umask(UMASK_FLAGS);

    // Check PID file; get read lock; ensure we can obtain write lock

    auto pidfile = Configuration::Instance().get<std::string>("pidfile");
    std::unique_ptr<PIDFile> pidf_ptr { new PIDFile(pidfile) };

    auto removeLockAndExit = [&pidf_ptr] () {
                                 pidf_ptr->cleanupWhenDone();
                                 exit(EXIT_FAILURE);
                             };

    if (pidf_ptr->isExecuting()) {
        auto pid = pidf_ptr->read();
        LOG_ERROR("Already running with PID=%1%", pid);
        removeLockAndExit();
    }

    try {
        pidf_ptr->lockRead();
        LOG_DEBUG("Obtained a read lock for the PID file; no other pxp-agent "
                  "daemon should be executing");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed get a read lock for the PID file: %1%", e.what());
        removeLockAndExit();
    }

    try {
        if (pidf_ptr->canLockWrite()) {
            LOG_DEBUG("It is possible to get a write lock for the PID file; no "
                      "other pxp-agent daemonization should be in progress");
        } else {
            LOG_ERROR("Cannot acquire the write lock for the PID file; please "
                      "ensure that there is no other pxp-agent instance executing");
            removeLockAndExit();
        }
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed to check if we can lock the PID file: %1%", e.what());
        removeLockAndExit();
    }

    // First fork - run in background

    auto first_child_pid = fork();

    switch (first_child_pid) {
        case -1:
            LOG_ERROR("Failed to perform the first fork; %1% (%2%)",
                      strerror(errno), errno);
            removeLockAndExit();
        case 0:
            // CHILD - will fork and exit soon
            LOG_DEBUG("First child spawned, with PID=%1%",
                      std::to_string(getpid()));
            break;
        default:
            // PARENT - wait for the child, to avoid a zombie process
            // and don't unlock the PID file
            waitpid(first_child_pid, nullptr, 0);

            // Exit with _exit(); note that after a fork(), only one
            // of parent and child should terminate with exit()
            _exit(EXIT_SUCCESS);
    }

    // Get the read lock

    try {
        pidf_ptr->lockRead();
        LOG_DEBUG("Obtained the read lock after first fork");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed get a read lock after first fork: %1%", e.what());
        removeLockAndExit();
    }

    // Create a new group session and become leader in order to
    // disassociate from a possible controlling terminal later

    if (setsid() == -1) {
        LOG_ERROR("Failed to create new session for first child process; "
                  "%1% (%2%)", strerror(errno), errno);
        removeLockAndExit();
    }

    // Prepare signal mask for the second fork in order to catch the
    // child signal: block the child signal for now in order to avoid
    // races and change signal disposition for SIGUSR1

    sigset_t block_mask, orig_mask, empty_mask;
    struct sigaction sa;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &block_mask, &orig_mask) == -1) {
        LOG_ERROR("Failed to set the signal mask after first fork");
        removeLockAndExit();
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = dumbSigHandler;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        LOG_ERROR("Failed to set SIGUSR1 disposition after first fork");
        removeLockAndExit();
    }

    // Second fork - the child won't be a session leader and won't be
    // associated with any controlling terminal

    auto second_child_pid = fork();

    switch (second_child_pid) {
        case -1:
            LOG_ERROR("Failed to perform the second fork; %1% (%2%)",
                      strerror(errno), errno);
            removeLockAndExit();
        case 0:
            // CHILD - will be the pxp-agent!
            break;
        default:
            // PARENT - wait for the child signal to avoid unlocking
            // the PID file
            sigemptyset(&empty_mask);

            if (sigsuspend(&empty_mask) == -1 && errno != EINTR) {
                LOG_ERROR("Unexpected error while waiting for pending signals "
                          "after second fork; %1% (%2%)", strerror(errno), errno);
            }

            _exit(EXIT_SUCCESS);
    }

    // Get read lock, signal the parent, and restore signal mask

    try {
        pidf_ptr->lockRead();
        LOG_DEBUG("Obtained the read lock after second fork");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed get a read lock after second fork: %1%", e.what());
        kill(getppid(), SIGUSR1);
        removeLockAndExit();
    }

    kill(getppid(), SIGUSR1);

    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) == -1) {
        LOG_ERROR("Failed to reset signal mask after second fork; "
                  "%1% (%2%)", strerror(errno), errno);
        removeLockAndExit();
    }

    auto agent_pid = getpid();
    LOG_DEBUG("Second child spawned, with PID=%1%", agent_pid);

    // Convert the read lock to a write lock and write PID to file

    LOG_DEBUG("Converting the read lock to write lock after second fork");

    try {
        pidf_ptr->lockWrite(true);  // blocking call
        LOG_DEBUG("Successfully converted read lock to write lock");
    } catch (const PIDFile::Error& e) {
        LOG_ERROR("Failed to convert to write lock after second fork: %1%",
                  e.what());
        removeLockAndExit();
    }

    pidf_ptr->write(agent_pid);

    // Change work directory

    if (chdir(DEFAULT_DAEMON_WORKING_DIR.data())) {
        LOG_ERROR("Failed to change work directory to '%1%'; %2% (%3%)",
                  DEFAULT_DAEMON_WORKING_DIR, strerror(errno), errno);
        removeLockAndExit();
    } else {
        LOG_DEBUG("Changed working directory to '%1%'", DEFAULT_DAEMON_WORKING_DIR);
    }

    // Change signal dispositions
    // HERE(ale): don't touch SIGUSR2; it's used to reopen the logfile

    for (auto s : std::vector<int> { SIGINT, SIGTERM, SIGQUIT }) {
        if (signal(s, sigHandler) == SIG_ERR) {
            LOG_ERROR("Failed to set signal handler for sig %1%", s);
            removeLockAndExit();
        }
    }

    signal(SIGCHLD, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // Redirect standard files; we always use boost::log anyway

    auto tmpin = freopen("/dev/null", "r", stdin);
    if (tmpin == nullptr) {
        LOG_WARNING("Failed to redirect stdin to /dev/null; %1% (%2%)",
                    strerror(errno), errno);
    }
    auto tmpout = freopen("/dev/null", "w", stdout);
    if (tmpout == nullptr) {
        LOG_WARNING("Failed to redirect stdout to /dev/null; %1% (%2%)",
                    strerror(errno), errno);
    }
    auto tmperr = freopen("/dev/null", "w", stderr);
    if (tmperr == nullptr) {
        LOG_WARNING("Failed to redirect stderr to /dev/null; %1% (%2%)",
                    strerror(errno), errno);
    }

    LOG_INFO("Daemonization completed; pxp-agent PID=%1%, PID lock file in '%2%'",
             agent_pid, pidfile);

    // Set PIDFile dtor to clean itself up and return pointer for RAII
    pidf_ptr->cleanupWhenDone();

    return pidf_ptr;
}

}  // namespace Util
}  // namespace PXPAgent
