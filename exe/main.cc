#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/file_utils.hpp>
#include <cthun-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.main"
#include <leatherman/logging/logging.hpp>

#include <fstream>

#include <horsewhisperer/horsewhisperer.h>

namespace CthunAgent {

namespace HW = HorseWhisperer;

int startAgent(std::vector<std::string> arguments) {
    std::string logfile { HW::GetFlag<std::string>("logfile") };
    std::ofstream file_stream;

    if (!logfile.empty()) {
        file_stream.open(logfile);
        leatherman::logging::setup_logging(file_stream);
    } else {
        leatherman::logging::setup_logging(std::cout);
        leatherman::logging::set_colorization(true);
    }

    switch (HW::GetFlag<int>("vlevel")) {
        case 0:
            leatherman::logging::set_level(leatherman::logging::log_level::info);
            break;
        case 1:
            leatherman::logging::set_level(leatherman::logging::log_level::debug);
            break;
        case 2:
            leatherman::logging::set_level(leatherman::logging::log_level::trace);
            break;
        default:
            leatherman::logging::set_level(leatherman::logging::log_level::trace);
    }

    try {
        Agent agent { HW::GetFlag<std::string>("module-dir"),
                      HW::GetFlag<std::string>("server"),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("ca")),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("cert")),
                      FileUtils::shellExpand(HW::GetFlag<std::string>("key")) };

        agent.start();
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
