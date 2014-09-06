#include "agent.h"

#include <cthun-client/src/log/log.h>

LOG_DECLARE_NAMESPACE("agent.main");

static const Cthun::Log::log_level DEFAULT_LOG_LEVEL { Cthun::Log::log_level::debug };
static std::ostream& DEFAULT_LOG_STREAM { std::cout };

int main(int argc, char *argv[]) {
    // TODO(ale): implement a Configuration module
    // TODO(ale): get arguments from command line

    Cthun::Log::configure_logging(DEFAULT_LOG_LEVEL, DEFAULT_LOG_STREAM);

    CthunAgent::Agent agent;
    //agent.run(argv[1], argv[2]);
    agent.connect_and_run();
    return 0;
}
