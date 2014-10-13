#include "src/agent/agent_endpoint.h"
#include "src/agent/errors.h"
#include "src/common/log.h"
#include "src/common/file_utils.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

LOG_DECLARE_NAMESPACE("agent_main");

namespace po = boost::program_options;

namespace Cthun {

//
// tokens
//

static const Common::Log::log_level DEFAULT_LOG_LEVEL { Common::Log::log_level::info };

// TODO(ale): allow configuring log and out files
static std::ostream& DEFAULT_LOG_STREAM = std::cout;

// TODO(ale): remove this; it's just for development
static const std::string DEFAULT_SERVER_URL { "wss://127.0.0.1:8090/cthun/" };
static std::string DEFAULT_CA { "./test-resources/ssl/ca/ca_crt.pem" };
static std::string DEFAULT_CERT {
    "./test-resources/ssl/certs/cthun-client.pem" };
static std::string DEFAULT_KEY {
    "./test-resources/ssl/private_keys/cthun-client.pem" };

//
// application options
//

struct AppOptions {
    bool help;
    bool debug;
    bool trace;
    std::string server;
    std::string ca;
    std::string cert;
    std::string key;

    AppOptions() : help { false } {}
};

//
// parse, process, and validate command line
//

AppOptions getAppOptions(int argc, char* argv[]) {
    po::options_description desc { "Allowed options" };
    po::variables_map vm;
    bool is_debug_level { false };
    bool is_trace_level { false };

    desc.add_options()
        ("help", "display help")
        ("debug", po::bool_switch(
            &is_debug_level), "enable logging at debug level")
        ("trace", po::bool_switch(
            &is_trace_level), "enable logging at trace level")
        ("server,s", po::value<std::string>()->default_value(
            DEFAULT_SERVER_URL), "cthun servers url")
        ("ca", po::value<std::string>()->default_value(
            DEFAULT_CA), "CA certificate")
        ("cert", po::value<std::string>()->default_value(
            DEFAULT_CERT), "cthun-agent certificate")
        ("key", po::value<std::string>()->default_value(
            DEFAULT_KEY), "cthun-agent private key");

    try {
        po::store(po::command_line_parser(argc, argv)
                  .options(desc).run(), vm);
        po::notify(vm);
    } catch (...) {
        std::cout << "Failed to parse the command line\n\n"
                  << desc << std::endl;
        throw Agent::request_error { "" };
    }

    AppOptions app_options;

    if (vm.count("help")) {
        std::cout << desc;
        app_options.help = true;
        return app_options;
    }

    app_options.debug  = is_debug_level;
    app_options.trace  = is_trace_level;
    app_options.server = vm["server"].as<std::string>();

    auto getFilePath = [&vm](std::string key) {
        return Cthun::Common::FileUtils::expandAsDoneByShell(
            vm[key].as<std::string>());
    };

    app_options.ca   = getFilePath("ca");
    app_options.cert = getFilePath("cert");
    app_options.key  = getFilePath("key");

    return app_options;
}

void processAndValidateAppOptions(AppOptions& app_options) {
    // Ensure all files exist

    std::vector<std::string> paths { app_options.ca,
                                     app_options.cert,
                                     app_options.key };

    for (auto p : paths) {
        if (!Cthun::Common::FileUtils::fileExists(p)) {
            throw Agent::request_error { "invalid certificate " + p };
        }
    }
}

//
// main
//

int main(int argc, char *argv[]) {
    AppOptions app_options;

    // parse command line

    try {
        app_options = getAppOptions(argc, argv);
    } catch (Agent::request_error) {
        return 1;
    }

    // return in case help has been displayed

    if (app_options.help) {
        return 0;
    }

    // configure logging

    // TODO(ale): do we need to configure facter logs?

    Common::Log::log_level log_level;

    if (app_options.trace) {
        log_level = Common::Log::log_level::trace;
    } else if (app_options.debug) {
        log_level = Common::Log::log_level::debug;
    } else {
        log_level = DEFAULT_LOG_LEVEL;
    }

    Cthun::Common::Log::configure_logging(log_level, DEFAULT_LOG_STREAM);

    // process and validate options

    try {
        processAndValidateAppOptions(app_options);
    } catch (Agent::request_error& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    // start the agent

    try {
        Agent::AgentEndpoint agent;

        agent.connectAndRun(app_options.server,
                            app_options.ca,
                            app_options.cert,
                            app_options.key);
    } catch (Agent::fatal_error&  e) {
        LOG_ERROR("fatal error: %1%", e.what());
        return 1;
    } catch (std::exception&  e) {
        LOG_ERROR("unexpected error: %1%", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("unexpected error");
        return 1;
    }

    return 0;
}

}  // namespace Cthun

int main(int argc, char** argv) {
    return Cthun::main(argc, argv);
}
