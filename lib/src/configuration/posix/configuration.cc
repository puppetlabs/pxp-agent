#include <pxp-agent/configuration.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration.posix.configuration"
#include <leatherman/logging/logging.hpp>

#include <stdexcept>
#include <signal.h>

namespace PXPAgent {

namespace lth_loc = leatherman::locale;

static void sigLogfileReopen(int signal) {
    LOG_INFO("Caught SIGUSR2 signal");
    try {
        Configuration::Instance().reopenLogfiles();
    } catch (const std::exception& e) {
        // NB(ale): Configuration::reopenLogfiles should be exception safe...
        LOG_ERROR("Failed to reopen logfile: {1}", e.what());
    }
}

void configure_platform_file_logging() {
    // Add the SIGUSR2 handler
    // HERE(ale): we expect that daemonize() will not touch this
    if (signal(SIGUSR2, sigLogfileReopen) == SIG_ERR) {
        throw Configuration::Error {
            lth_loc::translate("failed to set the SIGUSR2 handler") };
    } else {
        LOG_DEBUG("Successfully registered the SIGUSR2 handler to reopen "
                  "the logfile");
    }
}

}  // namespace PXPAgent
