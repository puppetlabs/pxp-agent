#include "src/agent.h"
#include "src/errors.h"
#include "src/file_utils.h"
#include "configuration.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.cthun_agent_main"
#include <leatherman/logging/logging.hpp>

#include <fstream>

#include <horsewhisperer/horsewhisperer.h>

namespace Log = leatherman::logging;

namespace CthunAgent {

namespace HW = HorseWhisperer;

int startAgent(std::vector<std::string> arguments   ) {
    Log::log_level log_level;

    switch (HW::GetFlag<int>("vlevel")) {
        case 0:
            log_level = Log::log_level::info;
            break;
        case 1:
            log_level = Log::log_level::debug;
            break;
        case 2:
            log_level = Log::log_level::trace;
            break;
        default:
            log_level = Log::log_level::trace;
    }

    std::ostream& console_stream = std::cout;
    std::string logfile { HW::GetFlag<std::string>("logfile") };
    std::ofstream file_stream;

    if (!logfile.empty()) {
        file_stream.open(logfile);
        Log::setup_logging(file_stream);
        Log::set_level(log_level);
    } else {
        Log::setup_logging(console_stream);
        Log::set_level(log_level);
    }

    try {
        Agent agent { HW::GetFlag<std::string>("module-dir"),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("ca")),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("cert")),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("key")) };

        agent.startAgent(HW::GetFlag<std::string>("server"));
    } catch (fatal_error& e) {
        LOG_ERROR("fatal error: %1%", e.what());
        return 1;
    } catch (std::exception& e) {
        LOG_ERROR("unexpected error: %1%", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("unexpected error");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    // manipulate argc and v to make start the default action.
    // TODO(ploubser): Add ability to specify default action to HorseWhisperer
    int modified_argc = argc + 1;
    char* modified_argv[modified_argc];
    char action[] = "start";

    for (int i = 0; i < argc; i++) {
        modified_argv[i] = argv[i];
    }
    modified_argv[modified_argc - 1] = action;

    Configuration::Instance().setStartFunction(startAgent);

    int parse_result = HW::PARSE_HELP;

    try {
        parse_result = Configuration::Instance().initialize(argc, argv);
    } catch(configuration_error e) {
        std::cout << "An error occurred while parsing your configuration.\n";
        std::cout << e.what() << std::endl;
        return 1;
    }

    switch (parse_result) {
        case HW::PARSE_OK:
            return HW::Start();
        case HW::PARSE_HELP:
            HorseWhisperer::ShowHelp();
            return 0;
        case HW::PARSE_VERSION:
            HorseWhisperer::ShowVersion();
            return 0;
        default:
            std::cout << "An unexpected code was returned when trying to parse"
                      << "command line arguments - " << parse_result
                      << ". Aborting" << std::endl;
            return 1;
    }
}

}  // namespace CthunAgent

int main(int argc, char** argv) {
    return CthunAgent::main(argc, argv);
}
