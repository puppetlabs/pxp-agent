#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/configuration.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.main"
#include <leatherman/logging/logging.hpp>

#include <fstream>

#include <horsewhisperer/horsewhisperer.h>

namespace CthunAgent {

namespace HW = HorseWhisperer;
namespace lth_file = leatherman::file_util;

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
        Agent agent { Configuration::Instance().get<std::string>("modules-dir"),
                      Configuration::Instance().get<std::string>("server"),
                      Configuration::Instance().get<std::string>("ca"),
                      Configuration::Instance().get<std::string>("cert"),
                      Configuration::Instance().get<std::string>("key"),
                      Configuration::Instance().get<std::string>("spool-dir") };

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

    HW::ParseResult parse_result;

    try {
        parse_result = Configuration::Instance().initialize(argc, argv);
    } catch (HW::flag_validation_error& e) {
        std::cout << "Invalid command line arguments:\n"
                  << e.what() << std::endl;
        return 1;
    } catch(configuration_error& e) {
        std::cout << "An error occurred while parsing your configuration.\n"
                  << e.what() << std::endl;
        return 1;
    }

    switch (parse_result) {
        case HW::ParseResult::OK:
            return HW::Start();
        case HW::ParseResult::HELP:
            HorseWhisperer::ShowHelp();
            return 0;
        case HW::ParseResult::VERSION:
            HorseWhisperer::ShowVersion();
            return 0;
        default:
            std::cout << "An unexpected code was returned when trying to parse"
                      << "command line arguments - "
                      << static_cast<int>(parse_result) << ". Aborting"
                      << std::endl;
            return 1;
    }
}

}  // namespace CthunAgent

int main(int argc, char** argv) {
    return CthunAgent::main(argc, argv);
}
