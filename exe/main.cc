#include <pxp-agent/agent.hpp>
#include <pxp-agent/configuration.hpp>

#include <pxp-agent/util/daemonize.hpp>

#include <cpp-pcp-client/util/thread.hpp>
#include <cpp-pcp-client/util/chrono.hpp>

#include <leatherman/file_util/file.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.main"
#include <leatherman/logging/logging.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <boost/nowide/args.hpp>
#include <boost/nowide/iostream.hpp>

#include <memory>

namespace PXPAgent {

namespace HW = HorseWhisperer;
namespace lth_file = leatherman::file_util;

// Start a thread that just busy waits to facilitate acceptance testing
void loopIdly() {
    PCPClient::Util::thread idle_thread {
        []() {
            for (;;) {
                PCPClient::Util::this_thread::sleep_for(
                    PCPClient::Util::chrono::milliseconds(5000));
            }
        } };
    idle_thread.join();
}

int startAgent(std::vector<std::string> arguments) {
#ifndef _WIN32
    std::unique_ptr<Util::PIDFile> pidf_ptr;
#endif

    try {
        // Using HW because configuration may not be initialized at this point
        if (!HW::GetFlag<bool>("foreground")) {
#ifdef _WIN32
            Util::daemonize();
#else
            // Store it for RAII
            // NB: pidf_ptr will be nullptr if already a daemon
            pidf_ptr = Util::daemonize();
#endif
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to daemonize: %1%", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR("Failed to daemonize");
        return EXIT_FAILURE;
    }

    bool success { true };
    if (!Configuration::Instance().valid()) {
        // pxp-agent will execute in uncofigured mode
        loopIdly();
    } else {
        try {
            Agent agent { Configuration::Instance().getAgentConfiguration() };
            agent.start();
        } catch (const Agent::Error& e) {
            success = false;
            LOG_ERROR("Fatal error: %1%", e.what());
        } catch (const std::exception& e) {
            success = false;
            LOG_ERROR("Unexpected error: %1%", e.what());
        } catch (...) {
            success = false;
            LOG_ERROR("Unexpected error");
        }
    }

#ifdef _WIN32
    Util::daemon_cleanup();
#endif
    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // Initialize

    // Fix args on Windows to be UTF-8
    boost::nowide::args arg_utf8(argc, argv);

    Configuration::Instance().initialize(startAgent);

    // Parse options

    HW::ParseResult parse_result { HW::ParseResult::OK };
    std::string err_msg {};

    try {
        parse_result = Configuration::Instance().parseOptions(argc, argv);
    } catch (const HW::horsewhisperer_error& e) {
        // Failed to validate action argument or flag
        err_msg = e.what();
    } catch(const Configuration::Error& e) {
        // Failed to parse the config file
        err_msg = e.what();
    }
    if (!err_msg.empty()) {
        boost::nowide::cout << err_msg
                            << "\nCannot start pxp-agent"
                            << std::endl;
        return EXIT_FAILURE;
    }

    switch (parse_result) {
        case HW::ParseResult::OK:
            break;
        case HW::ParseResult::HELP:
            // Show only the global section of HorseWhisperer's help
            HW::ShowHelp(false);
            return EXIT_SUCCESS;
        case HW::ParseResult::VERSION:
            HW::ShowVersion();
            return EXIT_SUCCESS;
        default:
            boost::nowide::cout << "An unexpected code was returned when trying "
                                << "to parse command line arguments - "
                                << static_cast<int>(parse_result)
                                << "\nCannot start pxp-agent"
                                << std::endl;
            return EXIT_FAILURE;
    }

    // Set up logging

    try {
        Configuration::Instance().setupLogging();
        LOG_INFO("pxp-agent logging has been initialized");
    } catch(const Configuration::Error& e) {
        boost::nowide::cout << "Failed to configure logging: " << e.what()
                            << "\nCannot start pxp-agent"
                            << std::endl;
        return EXIT_FAILURE;
    }

    // Validate options

    try {
        Configuration::Instance().validate();
        LOG_INFO("pxp-agent configuration has been validated");
    } catch(const Configuration::UnconfiguredError& e) {
        LOG_ERROR("WebSocket configuration error (%1%); pxp-agent will start "
                  "unconfigured and no connection will be attempted", e.what());
    } catch(const Configuration::Error& e) {
        LOG_ERROR("Fatal configuration error: %1%; cannot start pxp-agent",
                  e.what());
        return EXIT_FAILURE;
    }

    return HW::Start();
}

}  // namespace PXPAgent

int main(int argc, char** argv) {
    return PXPAgent::main(argc, argv);
}
