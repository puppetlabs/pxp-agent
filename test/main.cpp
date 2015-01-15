// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_RUNNER

#include "test/test.h"

#include "src/common/log.h"

int main(int argc, char* const argv[]) {
    // configure logging
    Cthun::Common::Log::configure_logging(Cthun::Common::Log::log_level::fatal,
                                          std::cout);

    // configure Session

    // TODO(ale): improve output by properly using reporters

    Catch::Session test_session;
    test_session.applyCommandLine(argc, argv);

    // To list the reporters
    // test_session.configData().listReporters = true;

    // Reporters: "xml", "junit", "console", "compact"
    test_session.configData().reporterName = "console";

    // ShowDurations::Always, ::Never, ::DefaultForReporter
    test_session.configData().showDurations = Catch::ShowDurations::Always;

    return test_session.run();
}
