#include <cthun-agent/agent.hpp>
#include <cthun-agent/configuration.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.main"
#include <leatherman/logging/logging.hpp>

#include <horsewhisperer/horsewhisperer.h>

namespace CthunAgent {

namespace HW = HorseWhisperer;
namespace lth_file = leatherman::file_util;

int startAgent(std::vector<std::string> arguments) {

    try {
        Agent agent { Configuration::Instance().get<std::string>("modules-dir"),
                      Configuration::Instance().get<std::string>("server"),
                      Configuration::Instance().get<std::string>("ca"),
                      Configuration::Instance().get<std::string>("cert"),
                      Configuration::Instance().get<std::string>("key"),
                      Configuration::Instance().get<std::string>("spool-dir") };

        agent.start();
    } catch (Agent::Error& e) {
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
    std::string err_msg {};

    try {
        parse_result = Configuration::Instance().initialize(argc, argv);
    } catch (HW::horsewhisperer_error& e) {
        // Failed to validate action argument or flag
        err_msg = e.what();
    } catch(Configuration::Error& e) {
        std::cout << "An unexpected error has occurred:\n";
        err_msg = e.what();
    }

    if (!err_msg.empty()) {
        std::cout << err_msg << "\nCannot start cthun-agent" << std::endl;
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
