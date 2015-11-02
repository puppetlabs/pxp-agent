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

// Exit code returned after a successful execution
static int PXP_AGENT_SUCCESS = 0;

// Exit code returned after a general failure
static int PXP_AGENT_GENERAL_FAILURE = 1;

// Exit code returned after a parsing failure
static int PXP_AGENT_PARSING_FAILURE = 2;

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
        return PXP_AGENT_GENERAL_FAILURE;
    } catch (...) {
        LOG_ERROR("Failed to daemonize");
        return PXP_AGENT_GENERAL_FAILURE;
    }

    int exit_code { PXP_AGENT_SUCCESS };
    if (!Configuration::Instance().valid()) {
        // pxp-agent will execute in uncofigured mode
        loopIdly();
    } else {
        try {
            Agent agent { Configuration::Instance().getAgentConfiguration() };
            agent.start();
        } catch (const Agent::WebSocketConfigurationError& e) {
            LOG_ERROR("WebSocket configuration error (%1%) - pxp-agent will "
                      "continue executing, but will not attempt to connect to "
                      "the PCP broker again", e.what());
            loopIdly();
        } catch (const Agent::FatalError& e) {
            exit_code = PXP_AGENT_GENERAL_FAILURE;
            LOG_ERROR("Fatal error: %1%", e.what());
        } catch (const std::exception& e) {
            exit_code = PXP_AGENT_GENERAL_FAILURE;
            LOG_ERROR("Unexpected error: %1%", e.what());
        } catch (...) {
            exit_code = PXP_AGENT_GENERAL_FAILURE;
            LOG_ERROR("Unexpected error");
        }
    }

#ifdef _WIN32
    Util::daemon_cleanup();
#endif
    return exit_code;
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
        // Try to set up logging with default settings
        try {
            Configuration::Instance().setupLogging();
            LOG_ERROR(err_msg);
        } catch(Configuration::Error) {
            // pass
        }
        boost::nowide::cout << err_msg
                            << "\nCannot start pxp-agent"
                            << std::endl;
        return PXP_AGENT_PARSING_FAILURE;
    }

    switch (parse_result) {
        case HW::ParseResult::OK:
            break;
        case HW::ParseResult::HELP:
            // Show only the global section of HorseWhisperer's help
            HW::ShowHelp(false);
            return PXP_AGENT_SUCCESS;
        case HW::ParseResult::VERSION:
            HW::ShowVersion();
            return PXP_AGENT_SUCCESS;
        default:
            boost::nowide::cout << "An unexpected code was returned when trying "
                                << "to parse command line arguments - "
                                << static_cast<int>(parse_result)
                                << "\nCannot start pxp-agent"
                                << std::endl;
            return PXP_AGENT_GENERAL_FAILURE;
    }

    // Set up logging

    try {
        Configuration::Instance().setupLogging();
        LOG_INFO("pxp-agent logging has been initialized");
    }  catch(const Configuration::Error& e) {
        boost::nowide::cout << "Failed to configure logging: " << e.what()
                            << "\nCannot start pxp-agent"
                            << std::endl;
        return PXP_AGENT_GENERAL_FAILURE;
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
        return PXP_AGENT_GENERAL_FAILURE;
    }

    return HW::Start();
}

}  // namespace PXPAgent

int main(int argc, char** argv) {
    return PXPAgent::main(argc, argv);
}
