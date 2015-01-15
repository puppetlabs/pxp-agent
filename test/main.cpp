// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_RUNNER

#include "test/test.h"

#include "src/common/log.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

std::string ROOT_PATH;

int main(int argc, char* const argv[]) {
    // set the global bin
    boost::filesystem::path root_path {
        boost::filesystem::canonical(
            boost::filesystem::system_complete(
                boost::filesystem::path(argv[0])).parent_path().parent_path())
    };

    ROOT_PATH = std::string(root_path.string());

    std::cout << "### main argv: " << argv[0] << std::endl;

    // configure logging
    Cthun::Common::Log::configure_logging(Cthun::Common::Log::log_level::fatal,
                                          std::cout);

    // configure Session

    // TODO(ale): improve output by properly using reporters
    // (dump the xml and use an external parser)

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
