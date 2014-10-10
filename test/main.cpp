#include "test/test.h"

#include "src/common/log.h"

int main(int argc, char** argv) {
    // Configure boost log
    Cthun::Common::Log::configure_logging(
        Cthun::Common::Log::log_level::fatal, std::cout);

    // Run all tests
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
