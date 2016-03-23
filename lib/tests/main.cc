// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_RUNNER

#include <catch.hpp>

// To enable log messages:
// #define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp-agent.test"
#include <leatherman/logging/logging.hpp>
#endif

int main(int argc, char* const argv[]) {
#ifdef ENABLE_LOGGING
    leatherman::logging::set_level(leatherman::logging::log_level::debug);
#endif

    // configure the Catch session and start it
    Catch::Session test_session;
    test_session.applyCommandLine(argc, argv);

    // To list the reporters use:
    // test_session.configData().listReporters = true;

    // Reporters: "xml", "junit", "console", "compact"
    test_session.configData().reporterName = "console";

    // ShowDurations::Always, ::Never, ::DefaultForReporter
    test_session.configData().showDurations = Catch::ShowDurations::Always;

    return test_session.run();
}
