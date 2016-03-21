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

// Exit code returned after invalid configuration
static int PXP_AGENT_CONFIGURATION_FAILURE = 3;

// Exit code returned after daemonization failure (only on POSIX)
static int PXP_AGENT_DAEMONIZATION_FAILURE = 4;

int startAgent(std::vector<std::string> arguments) {
#ifndef _WIN32
    std::unique_ptr<Util::PIDFile> pidf_ptr;
#endif

    try {
        if (!Configuration::Instance().get<bool>("foreground")) {
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
        return PXP_AGENT_DAEMONIZATION_FAILURE;
    } catch (...) {
        LOG_ERROR("Failed to daemonize");
        return PXP_AGENT_DAEMONIZATION_FAILURE;
    }

    int exit_code { PXP_AGENT_SUCCESS };

    try {
        Agent agent { Configuration::Instance().getAgentConfiguration() };
        agent.start();
    } catch (const Agent::PCPConfigurationError& e) {
        exit_code = PXP_AGENT_CONFIGURATION_FAILURE;
        LOG_ERROR("PCP configuration error: %1%", e.what());
    } catch (const Agent::WebSocketConfigurationError& e) {
        exit_code = PXP_AGENT_CONFIGURATION_FAILURE;
        LOG_ERROR("WebSocket configuration error: %1%", e.what());
    } catch (const Agent::Error& e) {
        exit_code = PXP_AGENT_GENERAL_FAILURE;
        LOG_ERROR("Fatal error: %1%", e.what());
    } catch (const std::exception& e) {
        exit_code = PXP_AGENT_GENERAL_FAILURE;
        LOG_ERROR("Unexpected error: %1%", e.what());
    } catch (...) {
        exit_code = PXP_AGENT_GENERAL_FAILURE;
        LOG_ERROR("Unexpected error");
    }

#ifdef _WIN32
    Util::daemon_cleanup();
#endif

    return exit_code;
}

static void tryLogError(const std::string& err_msg) {
    try {
        // Try to set up logging with default settings
        Configuration::Instance().setupLogging();
        LOG_ERROR(err_msg);
    } catch(Configuration::Error) {
        // pass
    }
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
        tryLogError(err_msg);
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
            // Unexpected; return the parsing failure exit code, since
            // pxp-agent was not configured
            err_msg = "Invalid parse result";
            tryLogError(err_msg);
            boost::nowide::cout << "An unexpected code was returned when trying "
                                << "to parse command line arguments - "
                                << static_cast<int>(parse_result)
                                << "\nCannot start pxp-agent"
                                << std::endl;
            return PXP_AGENT_PARSING_FAILURE;
    }

    // Set up logging

    try {
        Configuration::Instance().setupLogging();
        LOG_DEBUG("pxp-agent logging has been initialized");
    } catch(const Configuration::Error& e) {
        boost::nowide::cout << "Failed to configure logging: " << e.what()
                            << "\nCannot start pxp-agent"
                            << std::endl;
        return PXP_AGENT_CONFIGURATION_FAILURE;
    }

    // Validate options

    try {
        Configuration::Instance().validate();
        LOG_INFO("pxp-agent configuration has been validated");
    } catch(const Configuration::Error& e) {
        LOG_ERROR("Fatal configuration error: %1%; cannot start pxp-agent",
                  e.what());
        return PXP_AGENT_CONFIGURATION_FAILURE;
    }

    return HW::Start();
}

}  // namespace PXPAgent

int main(int argc, char** argv) {
    return PXPAgent::main(argc, argv);
}
