#include <pxp-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration.posix.configuration"
#include <leatherman/logging/logging.hpp>

#include <stdexcept>
#include <signal.h>

namespace PXPAgent {

static void sigLogfileReopen(int signal) {
    LOG_DEBUG("Caught SIGUSR2 signal - reopening log file");
    try {
        Configuration::Instance().reopenLogfile();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to reopen logfile: %1%", e.what());
    }
}

void configure_platform_file_logging() {
    // Add the SIGUSR2 handler
    // HERE(ale): we expect that daemonize() will not touch this
    if (signal(SIGUSR2, sigLogfileReopen) == SIG_ERR) {
        throw Configuration::Error { "failed to set the SIGUSR2 handler" };
    } else {
        LOG_DEBUG("Successfully registered the SIGUSR2 handler to reopen "
                  "the logfile");
    }
}

}  // namespace PXPAgent
