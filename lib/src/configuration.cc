#include <pxp-agent/configuration.hpp>

#include "version-inl.hpp"

#include <cpp-pcp-client/util/logging.hpp>

#include <leatherman/locale/locale.hpp>

#include <leatherman/file_util/file.hpp>

#include <leatherman/json_container/json_container.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>

#include <boost/nowide/iostream.hpp>

#ifdef _WIN32
    #include <leatherman/windows/system_error.hpp>
    #include <leatherman/windows/windows.hpp>
    #undef ERROR
    #include <boost/format.hpp>
    #include <Shlobj.h>
#endif

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace lth_jc = leatherman::json_container;
namespace lth_log = leatherman::logging;
namespace lth_loc = leatherman::locale;

#ifdef _WIN32
    namespace lth_w = leatherman::windows;
    static const fs::path DATA_DIR = []() {
        wchar_t szPath[MAX_PATH+1];
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath))) {
            throw Configuration::Error {
                (boost::format("failure getting Windows AppData directory: %1%")
                    % lth_w::system_error()).str() };
        }
        return fs::path(szPath) / "PuppetLabs" / "pxp-agent";
    }();

    static const fs::path DEFAULT_CONF_DIR { DATA_DIR / "etc" };
    const std::string DEFAULT_SPOOL_DIR { (DATA_DIR / "var" / "spool").string() };
    const std::string PID_DIR { (DATA_DIR / "var" / "run").string() };
    static const std::string DEFAULT_LOG_DIR { (DATA_DIR / "var" / "log").string() };

    static const std::string DEFAULT_MODULES_DIR = []() {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
            throw Configuration::Error { "failed to retrive pxp-agent binary path" };
        }
        // Go up twice, assuming the subtree from Puppet Specs:
        //      C:\Program Files\Puppet Labs\Puppet\pxp-agent
        //          bin
        //              pxp-agent.exe
        //          modules
        //              pxp-module-puppet
        try {
            return (fs::path(szPath).parent_path().parent_path() / "modules").string();
        } catch (const std::exception& e) {
            throw Configuration::Error {
                (boost::format("failed to determine the modules directory path: %1%")
                    % e.what()).str() };
        }
    }();
#else
    static const fs::path DEFAULT_CONF_DIR { "/etc/puppetlabs/pxp-agent" };
    const std::string DEFAULT_SPOOL_DIR { "/opt/puppetlabs/pxp-agent/spool" };
    const std::string PID_DIR { "/var/run/puppetlabs" };
    static const std::string DEFAULT_LOG_DIR { "/var/log/puppetlabs/pxp-agent" };
    static const std::string DEFAULT_MODULES_DIR { "/opt/puppetlabs/pxp-agent/modules" };
#endif

static const std::string DEFAULT_MODULES_CONF_DIR {
    (DEFAULT_CONF_DIR / "modules").string() };
static const std::string DEFAULT_CONFIG_FILE {
    (DEFAULT_CONF_DIR / "pxp-agent.conf").string() };

static const std::string AGENT_CLIENT_TYPE { "agent" };
const std::string LOGFILE_NAME { "pxp-agent.log" };

//
// Public interface
//

void Configuration::reset() {
    HW::Reset();
    setDefaultValues();
    initialized_ = false;
}

HW::ParseResult Configuration::initialize(int argc, char *argv[],
                                          bool enable_logging) {
    // Initialize boost filesystem's locale to a UTF-8 default.
    // Logging gets setup the same way via the default 2nd argument.
#if (!defined(__sun) && !defined(_AIX)) || !defined(__GNUC__)
    // Locale support in GCC on Solaris is busted, so skip it.
    fs::path::imbue(lth_loc::get_locale());
#endif

    setDefaultValues();

    HW::DefineAction("start", 0, false, "Start the agent (Default)",
                     "Start the agent", start_function_);

    // manipulate argc and v to make start the default action.
    // TODO(ploubser): Add ability to specify default action to HorseWhisperer
    int modified_argc = argc + 1;
    char* modified_argv[modified_argc];
    char action[] = "start";

    for (int i = 0; i < argc; i++) {
        modified_argv[i] = argv[i];
    }
    modified_argv[modified_argc - 1] = action;

    auto parse_result = HW::Parse(modified_argc, modified_argv);

    if (parse_result == HW::ParseResult::FAILURE
        || parse_result == HW::ParseResult::INVALID_FLAG) {
        throw Configuration::Error { "An error occurred while parsing cli options"};
    }

    if (parse_result == HW::ParseResult::OK) {
        // No further processing or user interaction are required if
        // the parsing outcome is HW::ParseResult::HELP or VERSION
        config_file_ = HW::GetFlag<std::string>("config-file");

        if (!config_file_.empty()) {
            parseConfigFile();
        }

        if (enable_logging) {
            setupLogging();
        }

        validateAndNormalizeConfiguration();
        setAgentConfiguration();

        initialized_ = true;
    }

    return parse_result;
}

void Configuration::setStartFunction(
        std::function<int(std::vector<std::string>)> start_function) {
    start_function_ = start_function;
}

void Configuration::validateAndNormalizeConfiguration() {
    // determine which of your values must be initalised
    if (HW::GetFlag<std::string>("broker-ws-uri").empty()) {
        throw Configuration::UnconfiguredError{ "broker-ws-uri value must be defined" };
    } else if (HW::GetFlag<std::string>("broker-ws-uri").find("wss://") != 0) {
        throw Configuration::UnconfiguredError { "broker-ws-uri value must start with wss://" };
    }

    if (HW::GetFlag<std::string>("ssl-ca-cert").empty()) {
        throw Configuration::UnconfiguredError { "ssl-ca-cert value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("ssl-ca-cert"))) {
        throw Configuration::UnconfiguredError { "ssl-ca-cert file not found" };
    }

    if (HW::GetFlag<std::string>("ssl-cert").empty()) {
        throw Configuration::UnconfiguredError { "ssl-cert value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("ssl-cert"))) {
        throw Configuration::UnconfiguredError { "ssl-cert file not found" };
    }

    if (HW::GetFlag<std::string>("ssl-key").empty()) {
        throw Configuration::UnconfiguredError { "ssl-key value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("ssl-key"))) {
        throw Configuration::UnconfiguredError { "ssl-key file not found" };
    }

    for (const auto& flag_name : std::vector<std::string> { "ssl-ca-cert",
                                                            "ssl-cert",
                                                            "ssl-key" }) {
        const auto& path = HW::GetFlag<std::string>(flag_name);
        HW::SetFlag<std::string>(flag_name, lth_file::tilde_expand(path));
    }

    if (HW::GetFlag<std::string>("spool-dir").empty())  {
        // Unexpected, since we have a default value for spool-dir
        throw Configuration::Error { "spool-dir must be defined" };
    } else {
        auto spool_dir = HW::GetFlag<std::string>("spool-dir");
        spool_dir = lth_file::tilde_expand(spool_dir);

        if (!fs::exists(spool_dir)) {
            throw Configuration::Error { "spool-dir does not exists" };
        } else if (!fs::is_directory(spool_dir)) {
            throw Configuration::Error { "--spool-dir '" + spool_dir +
                                         "' is not a directory'"};
        }
    }

    if (!HW::GetFlag<bool>("foreground")) {
        if (HW::GetFlag<bool>("console-logger")) {
            throw Configuration::Error { "must log to file when executing "
                                         "as a daemon" };
        }
    }
}

const Configuration::Agent& Configuration::getAgentConfiguration() const {
    return agent_configuration_;
}

bool Configuration::isInitialized() const {
    return initialized_;
}

void Configuration::reopenLogfile() const {
    if (!logfile_.empty()) {
        try {
            logfile_fstream_.close();
        } catch (const std::exception& e) {
            LOG_DEBUG("Failed to close logfile stream; already closed?");
        }
        try {
            logfile_fstream_.open(logfile_.c_str(), std::ios_base::app);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to open logfile stream");
        }
    }
}

//
// Private interface
//

Configuration::Configuration() : initialized_ { false },
                                 defaults_ {},
                                 config_file_ { "" },
                                 start_function_ {},
                                 agent_configuration_ {},
                                 logfile_ { "" },
                                 logfile_fstream_ {} {
    defineDefaultValues();
}

void Configuration::defineDefaultValues() {
    HW::SetAppName("pxp-agent");
    HW::SetHelpBanner("Usage: pxp-agent [options]");
    HW::SetVersion(std::string { PXP_AGENT_VERSION } + "\n");

    defaults_.insert(
        Option { "config-file",
                 Base_ptr { new Entry<std::string>(
                    "config-file",
                    "",
                    { "Config file, default: " + DEFAULT_CONFIG_FILE },
                    Types::String,
                    DEFAULT_CONFIG_FILE) } });

    defaults_.insert(
        Option { "broker-ws-uri",
                 Base_ptr { new Entry<std::string>(
                    "broker-ws-uri",
                    "b",
                    "WebSocket URI of the PCP broker",
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-ca-cert",
                 Base_ptr { new Entry<std::string>(
                    "ssl-ca-cert",
                    "",
                    "SSL CA certificate",
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-cert",
                 Base_ptr { new Entry<std::string>(
                    "ssl-cert",
                    "",
                    "pxp-agent SSL certificate",
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-key",
                 Base_ptr { new Entry<std::string>(
                    "ssl-key",
                    "",
                    "pxp-agent private SSL key",
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "logdir",
                 Base_ptr { new Entry<std::string>(
                    "logdir",
                    "",
                    { "Log directory, default: " + DEFAULT_LOG_DIR },
                    Types::String,
                    DEFAULT_LOG_DIR) } });

    defaults_.insert(
        Option { "loglevel",
                 Base_ptr { new Entry<std::string>(
                    "loglevel",
                    "",
                    "Valid options are 'none', 'trace', 'debug', 'info',"
                    "'warning', 'error' and 'fatal'. Defaults to 'info'",
                    Types::String,
                    "info") } });

    defaults_.insert(
        Option { "console-logger",
                 Base_ptr { new Entry<bool>(
                    "console-logger",
                    "",
                    "Show log messages only on console, default: false "
                    "(log to file)",
                    Types::Bool,
                    false) } });

    defaults_.insert(
        Option { "modules-dir",
                 Base_ptr { new Entry<std::string>(
                    "modules-dir",
                    "",
                    { "Modules directory, default"
#ifdef _WIN32
                      " (relative to the pxp-agent.exe path)"
#endif
                      ": " + DEFAULT_MODULES_DIR },
                    Types::String,
                    DEFAULT_MODULES_DIR) } });

    defaults_.insert(
        Option { "modules-config-dir",
                 Base_ptr { new Entry<std::string>(
                    "modules-config-dir",
                    "",
                    { "Module config files directory, default: " +
                    DEFAULT_MODULES_CONF_DIR },
                    Types::String,
                    DEFAULT_MODULES_CONF_DIR) } });

    defaults_.insert(
        Option { "spool-dir",
                 Base_ptr { new Entry<std::string>(
                    "spool-dir",
                    "",
                    { "Spool action results directory, default: " +
                    DEFAULT_SPOOL_DIR },
                    Types::String,
                    DEFAULT_SPOOL_DIR) } });

    defaults_.insert(
        Option { "foreground",
                 Base_ptr { new Entry<bool>(
                    "foreground",
                    "",
                    "Don't daemonize, default: false",
                    Types::Bool,
                    false) } });
}

void Configuration::setDefaultValues() {
    for (auto opt_idx = defaults_.get<Option::ByInsertion>().begin();
         opt_idx != defaults_.get<Option::ByInsertion>().end();
         ++opt_idx) {
        std::string flag_names { opt_idx->ptr->name };

        if (!opt_idx->ptr->aliases.empty()) {
            flag_names += " " + opt_idx->ptr->aliases;
        }

        switch (opt_idx->ptr->type) {
            case Integer:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<int>(flag_names, entry_ptr->help,
                                              entry_ptr->value,
                                              [entry_ptr] (int v) {
                                                  entry_ptr->configured = true;
                                              });
                }
                break;
            case Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<bool>(flag_names, entry_ptr->help,
                                               entry_ptr->value,
                                               [entry_ptr] (bool v) {
                                                   entry_ptr->configured = true;
                                               });
                }
                break;
            case Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<double>(flag_names, entry_ptr->help,
                                                 entry_ptr->value,
                                                 [entry_ptr] (double v) {
                                                     entry_ptr->configured = true;
                                                 });
                }
                break;
            default:
                {
                    Entry<std::string>* entry_ptr = (Entry<std::string>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<std::string>(flag_names, entry_ptr->help,
                                                      entry_ptr->value,
                                                      [entry_ptr] (std::string v) {
                                                          entry_ptr->configured = true;
                                                      });
                }
        }
    }
}

void Configuration::parseConfigFile() {
    lth_jc::JsonContainer config_json;

    if (!lth_file::file_readable(config_file_)) {
      return;
    }

    try {
        config_json = lth_jc::JsonContainer(lth_file::read(config_file_));
    } catch (lth_jc::data_parse_error& e) {
        throw Configuration::Error { "cannot parse config file; invalid JSON" };
    }

    if (config_json.type() != lth_jc::DataType::Object) {
        throw Configuration::Error { "invalid config file content; not a "
                                     "JSON object" };
    }

    for (const auto& key : config_json.keys()) {
        const auto& opt_idx = defaults_.get<Option::ByName>().find(key);

        if (opt_idx != defaults_.get<Option::ByName>().end()) {
            if (opt_idx->ptr->configured) {
                continue;
            }

            switch (opt_idx->ptr->type) {
                case Integer:
                    if (config_json.type(key) == lth_jc::DataType::Int) {
                        HW::SetFlag<int>(key, config_json.get<int>(key));
                    } else {
                        std::string err { "field '" + key + "' must be of "
                                          "type Integer" };
                        throw Configuration::Error { err };
                    }
                    break;
                case Bool:
                    if (config_json.type(key) == lth_jc::DataType::Bool) {
                        HW::SetFlag<bool>(key, config_json.get<bool>(key));
                    } else {
                        std::string err { "field '" + key + "' must be of "
                                          "type Bool" };
                        throw Configuration::Error { err };
                    }
                    break;
                case Double:
                    if (config_json.type(key) == lth_jc::DataType::Double) {
                        HW::SetFlag<double>(key, config_json.get<double>(key));
                    } else {
                        std::string err { "field '" + key + "' must be of "
                                          "type Double" };
                        throw Configuration::Error { err };
                    }
                    break;
                default:
                    if (config_json.type(key) == lth_jc::DataType::String) {
                        auto val = config_json.get<std::string>(key);
                        HW::SetFlag<std::string>(key, val);
                    } else {
                        std::string err { "field '" + key + "' must be of "
                                          "type String" };
                        throw Configuration::Error { err };
                    }
            }
        } else {
            std::string err { "field '" + key + "' is not a valid "
                              "configuration variable" };
            throw Configuration::Error { err };
        }
    }
}

static void validateLogDirPath(fs::path logdir_path) {
    if (fs::exists(logdir_path)) {
        if (!fs::is_directory(logdir_path)) {
            throw Configuration::Error { "log directory is not a directory" };
        }
    } else {
        throw Configuration::Error { "--logdir '" + logdir_path.string() +
                                     "' doesn't exist" };
    }
}

void Configuration::setupLogging() {
    auto console_logger = HW::GetFlag<bool>("console-logger");
    auto loglevel = HW::GetFlag<std::string>("loglevel");
    std::ostream *stream = nullptr;

    if (!console_logger) {
        // We should log on file
        auto logdir = HW::GetFlag<std::string>("logdir");

        if (logdir.empty()) {
            throw Configuration::Error { "must specify either log directory or "
                                         "console-logger flag" };
        }

        fs::path logdir_path { lth_file::tilde_expand(logdir) };

        // NOTE(ale): we must validate the logifle path since we set
        // up logging before calling validateAndNormalizeConfiguration
        validateLogDirPath(logdir_path);

        logfile_ = (logdir_path / LOGFILE_NAME).string();
        logfile_fstream_.open(logfile_.c_str(), std::ios_base::app);
        stream = &logfile_fstream_;
    } else {
        // Log on stdout by default
        stream = &boost::nowide::cout;
    }

    lth_log::log_level lvl = lth_log::log_level::none;

    try {
        // NOTE(ale): ignoring HorseWhisperer's vlevel ("-v" flag)
        const std::map<std::string, lth_log::log_level> option_to_log_level {
            { "none", lth_log::log_level::none },
            { "trace", lth_log::log_level::trace },
            { "debug", lth_log::log_level::debug },
            { "info", lth_log::log_level::info },
            { "warning", lth_log::log_level::warning },
            { "error", lth_log::log_level::error },
            { "fatal", lth_log::log_level::fatal }
        };
        lvl = option_to_log_level.at(loglevel);
    } catch (const std::out_of_range& e) {
        throw Configuration::Error { "invalid log level: '" + loglevel + "'" };
    }

    // Configure logging for pxp-agent
    lth_log::setup_logging(*stream);
    lth_log::set_level(lvl);

#ifdef DEV_LOG_COLOR
    // Enable colorozation anyway (development setting - it helps
    // debugging PXP message workflow, but it will add useless shell
    // control sequences to log entries on file)
    lth_log::set_colorization(true);
    bool force_colorization = true;
#else
    bool force_colorization = false;
#endif  // DEV_LOG_COLOR

    // Configure logging for cpp-pcp-client
    PCPClient::Util::setupLogging(*stream, force_colorization, loglevel);

    if (!console_logger) {
        // Configure platform-specific things for file logging
        // NB: we do that after setting up lth_log in order to log in
        //     case of failure
        configure_platform_file_logging();
    }
}

void Configuration::setAgentConfiguration() {
    agent_configuration_ = Configuration::Agent {
        HW::GetFlag<std::string>("modules-dir"),
        HW::GetFlag<std::string>("broker-ws-uri"),
        HW::GetFlag<std::string>("ssl-ca-cert"),
        HW::GetFlag<std::string>("ssl-cert"),
        HW::GetFlag<std::string>("ssl-key"),
        HW::GetFlag<std::string>("spool-dir"),
        HW::GetFlag<std::string>("modules-config-dir"),
        AGENT_CLIENT_TYPE };
}

}  // namespace PXPAgent
