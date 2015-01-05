// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_RUNNER

#include "test/test.h"

#include "src/common/log.h"

int main(int argc, char* const argv[]) {
    Cthun::Common::Log::configure_logging(Cthun::Common::Log::log_level::fatal,
                                          std::cout);
    return Catch::Session().run(argc, argv);
}
