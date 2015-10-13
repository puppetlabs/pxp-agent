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
#include <cpp-pcp-client/util/thread.hpp>
#include <cpp-pcp-client/util/chrono.hpp>

namespace PXPAgent {

namespace HW = HorseWhisperer;
namespace lth_file = leatherman::file_util;

int startAgent(std::vector<std::string> arguments) {
#ifndef _WIN32
    std::unique_ptr<Util::PIDFile> pidf_ptr;

    try {
        // Using HW because configuration may not be initialized at this point
        if (!HW::GetFlag<bool>("foreground")) {
            // Store it for RAII
            // NB: pidf_ptr will be nullptr if already a daemon
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

    if (!Configuration::Instance().isInitialized()) {
        // Start a thread that just busy waits to facilitate
        // acceptance testing
        PCPClient::Util::thread idle_thread {
            []() {
                LOG_WARNING("pxp-agent started unconfigured. No connection can "
                            "be established");
                for (;;) {
                    PCPClient::Util::this_thread::sleep_for(PCPClient::Util::chrono::milliseconds(5000));
                }
            } };
        idle_thread.join();
        return 0;
    }

    bool success { false };
    const auto& agent_configuration =
        Configuration::Instance().getAgentConfiguration();

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

    try {
        parse_result = Configuration::Instance().initialize(argc, argv);
    } catch (const HW::horsewhisperer_error& e) {
        // Failed to validate action argument or flag
        std::cout << e.what() << "\nCannot start pxp-agent'n";
        return 1;
    } catch(const Configuration::UnconfiguredError& e) {
        std::cout << "A configuration error has occurred: " << e.what() << std::endl;
        std::cout << "pxp-agent will start unconfigured\n";
    } catch(const Configuration::Error& e) {
        std::cout << "A configuration error has occurred: " << e.what() << std::endl;
        std::cout << "pxp-agent cannot start" << std::endl;
        return 1;
    }

    switch (parse_result) {
        case HW::ParseResult::OK:
            return HW::Start();
        case HW::ParseResult::HELP:
            // Show only the global section of HorseWhisperer's help
            HorseWhisperer::ShowHelp(false);
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
