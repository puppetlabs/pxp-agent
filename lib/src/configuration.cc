#include <pxp-agent/configuration.hpp>

#include "version-inl.hpp"

#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#include <leatherman/json_container/json_container.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration"
#include <leatherman/logging/logging.hpp>

#include <fstream>

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
    static const std::string PID_DIR { (DATA_DIR / "var" / "run").string() };
    static const std::string DEFAULT_LOG_DIR { (DATA_DIR / "var" / "log").string() };
#else
    static const fs::path DEFAULT_CONF_DIR { "/etc/puppetlabs/pxp-agent" };
    const std::string DEFAULT_SPOOL_DIR { "/opt/puppetlabs/pxp-agent/spool" };
    static const std::string PID_DIR { "/var/run/puppetlabs" };
    static const std::string DEFAULT_LOG_DIR { "/var/log/puppetlabs/pxp-agent" };
#endif


// TODO(ale): temporary fix; update this for CTH-373
static const std::string DEFAULT_MODULES_DIR {
    (fs::current_path() / "modules").string() };


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
        setNonExposedOptions();
    }

    initialized_ = true;

    return parse_result;
}

void Configuration::setStartFunction(
        std::function<int(std::vector<std::string>)> start_function) {
    start_function_ = start_function;
}

void Configuration::validateAndNormalizeConfiguration() {
    // determine which of your values must be initalised
    if (HW::GetFlag<std::string>("server").empty()) {
        throw Configuration::Error { "server value must be defined" };
    } else if (HW::GetFlag<std::string>("server").find("wss://") != 0) {
        throw Configuration::Error { "server value must start with wss://" };
    }

    if (HW::GetFlag<std::string>("ca").empty()) {
        throw Configuration::Error { "ca value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("ca"))) {
        throw Configuration::Error { "ca file not found" };
    }

    if (HW::GetFlag<std::string>("cert").empty()) {
        throw Configuration::Error { "cert value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("cert"))) {
        throw Configuration::Error { "cert file not found" };
    }

    if (HW::GetFlag<std::string>("key").empty()) {
        throw Configuration::Error { "key value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("key"))) {
        throw Configuration::Error { "key file not found" };
    }

    for (const auto& flag_name : std::vector<std::string> { "ca", "cert", "key" }) {
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
            LOG_INFO("Creating spool directory '%1%'", spool_dir);
            try {
                fs::create_directories(spool_dir);
            } catch (const fs::filesystem_error& e) {
                std::string err_msg { "failed to create the spool directory: " };
                throw Configuration::Error { err_msg + e.what() };
            }
        } else if (!fs::is_directory(spool_dir)) {
            throw Configuration::Error { "not a spool directory: " + spool_dir };
        }
    }

    if (HW::GetFlag<bool>("daemonize")) {
        if (HW::GetFlag<bool>("console-logger")) {
            throw Configuration::Error { "must log to file when executing "
                                         "as a daemon" };
        }
    }
}

const Configuration::Agent& Configuration::getAgentConfiguration() const {
    return agent_configuration_;
}

//
// Private interface
//

Configuration::Configuration() : initialized_ { false },
                                 defaults_ {},
                                 config_file_ { "" },
                                 start_function_ {},
                                 agent_configuration_ {} {
    defineDefaultValues();
}

void Configuration::defineDefaultValues() {
    HW::SetAppName("pxp-agent");
    HW::SetHelpBanner("Usage: pxp-agent [options]");
    HW::SetVersion(std::string { PXP_AGENT_VERSION } + "\n");

    defaults_.insert(std::pair<std::string, Base_ptr>("server", Base_ptr(
        new Entry<std::string>("server",
                               "s",
                               "PCP server URL",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("ca", Base_ptr(
        new Entry<std::string>("ca",
                               "",
                               "CA certificate",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("cert", Base_ptr(
        new Entry<std::string>("cert",
                               "",
                               "pxp-agent certificate",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("key", Base_ptr(
        new Entry<std::string>("key",
                               "",
                               "pxp-agent private key",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("logdir", Base_ptr(
        new Entry<std::string>("logdir",
                               "",
                               { "Log directory, default: " + DEFAULT_LOG_DIR },
                               Types::String,
                               DEFAULT_LOG_DIR))));

    defaults_.insert(std::pair<std::string, Base_ptr>("loglevel", Base_ptr(
        new Entry<std::string>("loglevel",
                               "",
                               "Log level; defaults to 'info'",
                               Types::String,
                               "info"))));

    defaults_.insert(std::pair<std::string, Base_ptr>("console-logger", Base_ptr(
        new Entry<bool>("console-logger",
                        "",
                        "Show log messages only on console, default: false "
                        "(log to file)",
                        Types::Bool,
                        false))));

    defaults_.insert(std::pair<std::string, Base_ptr>("config-file", Base_ptr(
        new Entry<std::string>("config-file",
                               "",
                               { "Config file, default: " + DEFAULT_CONFIG_FILE },
                               Types::String,
                               DEFAULT_CONFIG_FILE))));

    defaults_.insert(std::pair<std::string, Base_ptr>("spool-dir", Base_ptr(
        new Entry<std::string>("spool-dir",
                               "",
                               { "Spool action results directory, default: " +
                                    DEFAULT_SPOOL_DIR },
                               Types::String,
                               DEFAULT_SPOOL_DIR))));

    defaults_.insert(std::pair<std::string, Base_ptr>("modules-config-dir", Base_ptr(
        new Entry<std::string>("modules-config-dir",
                               "",
                               { "Module config files directory, default: " +
                                    DEFAULT_MODULES_CONF_DIR },
                               Types::String,
                               DEFAULT_MODULES_CONF_DIR))));

    defaults_.insert(std::pair<std::string, Base_ptr>("modules-dir", Base_ptr(
        new Entry<std::string>("modules-dir",
                               "",
                               { "Dirctory with modules scripts, default: " +
                                    DEFAULT_MODULES_DIR },
                               Types::String,
                               DEFAULT_MODULES_DIR))));

    defaults_.insert(std::pair<std::string, Base_ptr>("daemonize", Base_ptr(
        new Entry<bool>("daemonize",
                        "",
                        "Execute as a daemon",
                        Types::Bool,
                        true))));
}

void Configuration::setDefaultValues() {
    for (const auto& entry : defaults_) {
        std::string flag_names = entry.second->name;

        if (!entry.second->aliases.empty()) {
            flag_names += " " + entry.second->aliases;
        }

        switch (entry.second->type) {
            case Integer:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) entry.second.get();
                    HW::DefineGlobalFlag<int>(flag_names, entry_ptr->help,
                                              entry_ptr->value,
                                              [entry_ptr] (int v) {
                                                  entry_ptr->configured = true;
                                              });
                }
                break;
            case Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) entry.second.get();
                    HW::DefineGlobalFlag<bool>(flag_names, entry_ptr->help,
                                               entry_ptr->value,
                                               [entry_ptr] (bool v) {
                                                   entry_ptr->configured = true;
                                               });
                }
                break;
            case Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) entry.second.get();
                    HW::DefineGlobalFlag<double>(flag_names, entry_ptr->help,
                                                 entry_ptr->value,
                                                 [entry_ptr] (double v) {
                                                     entry_ptr->configured = true;
                                                 });
                }
                break;
            default:
                {
                    Entry<std::string>* entry_ptr = (Entry<std::string>*) entry.second.get();
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

    // TODO(ale): update this for CTH-380
    if (!lth_file::file_readable(config_file_)) {
        throw Configuration::Error { "config file '" + config_file_
                                     + "' doesn't exist" };
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
        try {
            const auto& entry = defaults_.at(key);

            if (!entry->configured) {
                switch (entry->type) {
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
            }
        } catch (const std::out_of_range& e) {
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
        try {
            fs::create_directories(logdir_path);
        } catch (const fs::filesystem_error& e) {
            std::string err_msg { "failed to create log directory: " };
            throw Configuration::Error { err_msg + e.what() };
        }
    }
}

void Configuration::setupLogging() {
    auto console_logger = HW::GetFlag<bool>("console-logger");
    auto loglevel = HW::GetFlag<std::string>("loglevel");

    if (!console_logger) {
        auto logdir = HW::GetFlag<std::string>("logdir");

        if (logdir.empty()) {
            throw Configuration::Error { "must specify either log directory or "
                                         "console-logger flag" };
        }

        fs::path logdir_path { lth_file::tilde_expand(logdir) };

        // NOTE(ale): we must validate the logifle path since we set
        // up logging before calling validateAndNormalizeConfiguration
        validateLogDirPath(logdir_path);

        auto logfile = (logdir_path / LOGFILE_NAME).string();
        static std::ofstream file_stream {};
        file_stream.open(logfile, std::ios_base::app);
        lth_log::setup_logging(file_stream);
#ifdef LOG_COLOR
        lth_log::set_colorization(true);
#endif  // LOG_COLOR
    } else {
        // Log on stdout by default
        lth_log::setup_logging(std::cout);
        lth_log::set_colorization(true);
    }

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

        lth_log::set_level(option_to_log_level.at(loglevel));
    } catch (const std::out_of_range& e) {
        throw Configuration::Error { "invalid log level: '" + loglevel + "'" };
    }
}

void Configuration::setAgentConfiguration() {
    agent_configuration_ = Configuration::Agent {
        HW::GetFlag<std::string>("modules-dir"),
        HW::GetFlag<std::string>("server"),
        HW::GetFlag<std::string>("ca"),
        HW::GetFlag<std::string>("cert"),
        HW::GetFlag<std::string>("key"),
        HW::GetFlag<std::string>("spool-dir"),
        HW::GetFlag<std::string>("modules-config-dir"),
        AGENT_CLIENT_TYPE };
}

void Configuration::setNonExposedOptions() {
    HW::DefineGlobalFlag<std::string>("pid-dir",
                                      "",
                                      PID_DIR,
                                      nullptr);
}

}  // namespace PXPAgent
