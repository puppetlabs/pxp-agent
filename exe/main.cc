#include <pxp-agent/agent.hpp>
#include <pxp-agent/configuration.hpp>

#ifndef _WIN32
#include <pxp-agent/util/posix/pid_file.hpp>
#include <pxp-agent/util/posix/daemonize.hpp>
#endif

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.main"
#include <leatherman/logging/logging.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <memory>

namespace PXPAgent {

namespace HW = HorseWhisperer;
namespace lth_file = leatherman::file_util;

int startAgent(std::vector<std::string> arguments) {
    const auto& agent_configuration =
        Configuration::Instance().getAgentConfiguration();

#ifndef _WIN32
    std::unique_ptr<Util::PIDFile> pidf_ptr;

    try {
        if (Configuration::Instance().get<bool>("daemonize")) {
            // Store it for RAII
            pidf_ptr = Util::daemonize();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to daemonize: %1%", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Failed to daemonize");
        return 1;
    }
#endif

    bool success { false };

    try {
        Agent agent { agent_configuration };
        agent.start();
        success = true;
    } catch (const Agent::Error& e) {
        LOG_ERROR("Fatal error: %1%", e.what());
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error: %1%", e.what());
    } catch (...) {
        LOG_ERROR("Unexpected error");
    }

    return (success ? 0 : 1);
}

int main(int argc, char *argv[]) {
    Configuration::Instance().setStartFunction(startAgent);

    HW::ParseResult parse_result;
    std::string err_msg {};

    try {
        parse_result = Configuration::Instance().initialize(argc, argv);
    } catch (const HW::horsewhisperer_error& e) {
        // Failed to validate action argument or flag
        err_msg = e.what();
    } catch(const Configuration::Error& e) {
        std::cout << "A configuration error has occurred:\n  ";
        err_msg = e.what();
    }

    if (!err_msg.empty()) {
        std::cout << err_msg << "\nCannot start pxp-agent" << std::endl;
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

}  // namespace PXPAgent

int main(int argc, char** argv) {
    return PXPAgent::main(argc, argv);
}
