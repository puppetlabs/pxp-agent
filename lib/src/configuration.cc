#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>

#include "version-inl.hpp"

#include <hocon/config.hpp>

#include <cpp-pcp-client/util/logging.hpp>
#include <cpp-pcp-client/connector/client_metadata.hpp>  // validate SSL certs
#include <cpp-pcp-client/connector/errors.hpp>

#include <leatherman/locale/locale.hpp>

#include <leatherman/file_util/file.hpp>

#include <leatherman/util/strings.hpp>
#include <leatherman/util/uri.hpp>

#include <leatherman/json_container/json_container.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/nowide/iostream.hpp>

#ifdef _WIN32
    #include <leatherman/windows/file_util.hpp>
    #include <leatherman/windows/system_error.hpp>
    #include <leatherman/windows/user.hpp>
    #include <leatherman/windows/windows.hpp>
    #undef ERROR
    #include <Shlobj.h>
#else
    #include <leatherman/util/environment.hpp>
#endif

namespace PXPAgent {

namespace HW = HorseWhisperer;
namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_log  = leatherman::logging;
namespace lth_loc  = leatherman::locale;
namespace lth_util = leatherman::util;

//
// Default values
//

#ifdef _WIN32
    namespace lth_w = leatherman::windows;
    static const fs::path DATA_DIR = []() {
        if (!lth_w::user::is_admin()) {
            auto home = lth_w::user::home_dir();
            if (!home.empty()) {
                return fs::path(home) / ".puppetlabs";
            }
            throw Configuration::Error { lth_loc::format("failure getting HOME directory") };
        } else {
            return fs::path();
        }
    }();

    static const fs::path& sys_dir() {
        try {
            static auto ddir = fs::path(lth_w::file_util::get_programdata_dir()) / "PuppetLabs" / "pxp-agent";
            return ddir;
        } catch (lth_w::file_util::unknown_folder_exception &e) {
            throw Configuration::Error {
                lth_loc::format("failure getting Windows AppData directory: {1}", e.what()) };
        }
    }

    static const std::string DEFAULT_MODULES_DIR = []() {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
            throw Configuration::Error {
                lth_loc::translate("failed to retrive pxp-agent binary path") };
        }
        // Go up three times, assuming the subtree from Puppet Specs:
        //      C:\Program Files\Puppet Labs\Puppet
        //          puppet
        //              bin
        //                  pxp-agent.exe
        //          pxp-agent
        //              modules
        //                  pxp-module-puppet
        try {
            return (fs::path(szPath).parent_path().parent_path().parent_path() / "pxp-agent" / "modules").string();
        } catch (const std::exception& e) {
            throw Configuration::Error {
                lth_loc::format("failed to determine the modules directory path: {1}",
                                e.what()) };
        }
    }();

    static fs::path conf_dir() { return sys_dir() / "etc"; }
    static fs::path log_dir() { return sys_dir() / "var" / "log"; }
    static std::string spool_dir() { return (sys_dir() / "var" / "spool").string(); }
    static std::string cache_dir() { return (sys_dir() / "tasks-cache").string(); }
#else
    static const fs::path DATA_DIR = []() {
        if (getuid()) {
            std::string home;
            if (lth_util::environment::get("HOME", home)) {
                return fs::path(home) / ".puppetlabs";
            }
            throw Configuration::Error { lth_loc::format("failure getting HOME directory") };
        } else {
            return fs::path();
        }
    }();

    static const std::string DEFAULT_PID_FILE { DATA_DIR.empty() ?
        std::string("/var/run/puppetlabs/pxp-agent.pid") : (DATA_DIR / "var" / "run" / "pxp-agent.pid").string() };
    static const std::string DEFAULT_MODULES_DIR { "/opt/puppetlabs/pxp-agent/modules" };

    const char* APPLICATION_NAME = "pxp-agent";

    static fs::path conf_dir()     { return "/etc/puppetlabs/pxp-agent"; }
    static fs::path log_dir()      { return "/var/log/puppetlabs/pxp-agent"; }
    static std::string spool_dir() { return "/opt/puppetlabs/pxp-agent/spool"; }
    static std::string cache_dir() { return "/opt/puppetlabs/pxp-agent/tasks-cache"; }
#endif

// DATA_DIR defines the non-root data directory. Functions define the system default.
static const fs::path DEFAULT_CONF_DIR { DATA_DIR.empty() ? conf_dir() : (DATA_DIR / "etc" / "pxp-agent") };
static const fs::path DEFAULT_LOG_DIR { DATA_DIR.empty() ? log_dir() : (DATA_DIR / "var" / "log") };
const std::string DEFAULT_SPOOL_DIR { DATA_DIR.empty() ?
    spool_dir() : (DATA_DIR / "opt" / "pxp-agent" / "spool").string() };
static const std::string DEFAULT_TASK_CACHE_DIR { DATA_DIR.empty() ?
    cache_dir() : (DATA_DIR / "opt" / "pxp-agent" / "tasks-cache").string() };

static const std::string DEFAULT_LOG_FILE { (DEFAULT_LOG_DIR / "pxp-agent.log").string() };
static const std::string DEFAULT_PCP_ACCESS_FILE { (DEFAULT_LOG_DIR / "pcp-access.log").string() };

static const std::string DEFAULT_MODULES_CONF_DIR { (DEFAULT_CONF_DIR / "modules").string() };
static const std::string DEFAULT_CONFIG_FILE { (DEFAULT_CONF_DIR / "pxp-agent.conf").string() };
static const std::string DEFAULT_PCP_VERSION { "1" };
static const std::string DEFAULT_DIR_PURGE_TTL { "14d" };

static const std::string AGENT_CLIENT_TYPE { "agent" };

const fs::perms NIX_FILE_PERMS { fs::owner_read | fs::owner_write | fs::group_read };
const fs::perms NIX_DIR_PERMS  { NIX_FILE_PERMS | fs::owner_exe | fs::group_exe };

// Helper function
static void check_and_create_dir(const fs::path& dir_path,
                                 const std::string& opt,
                                 const bool& create)
{
    if (!fs::exists(dir_path)) {
        if (create) {
            try {
                LOG_INFO("Creating the {1} '{2}'", opt, dir_path.string());
                fs::create_directories(dir_path);
                fs::permissions(dir_path, NIX_DIR_PERMS);
            } catch (const std::exception& e) {
                LOG_DEBUG("Failed to create the {1} '{2}': {3}",
                          opt, dir_path.string(), e.what());
                throw Configuration::Error {
                    lth_loc::format("failed to create the {1} '{2}'",
                                    opt, dir_path.string()) };
            }
        } else {
            throw Configuration::Error {
                lth_loc::format("the {1} '{2}' does not exist",
                                opt, dir_path.string()) };
        }
    } else if (!fs::is_directory(dir_path)) {
        throw Configuration::Error {
                lth_loc::format("the {1} '{2}' is not a directory",
                                opt, dir_path.string()) };
    }
}

//
// Public interface
//

void Configuration::initialize(
        std::function<int(std::vector<std::string>)> start_function)
{
    // Ensure the state is reset (useful for testing)
    HW::Reset();
    HW::SetAppName("pxp-agent");
    HW::SetHelpBanner(lth_loc::translate("Usage: pxp-agent [options]"));
    HW::SetVersion(std::string { PXP_AGENT_VERSION } + "\n");
    broker_ws_uris_.clear();
    valid_ = false;

    // Initialize boost filesystem's locale to a UTF-8 default.
    // Logging gets setup the same way via the default 2nd argument.
#ifdef LEATHERMAN_USE_LOCALES
    // Locale support in GCC on Solaris is busted, so skip it.
    fs::path::imbue(lth_loc::get_locale());
#endif

    setDefaultValues();

    // NOTE(ale): don't need to translate this (forced default action)
    HW::DefineAction("start",                       // action name
                     0,                             // arity
                     false,                         // chainable
                     "Start the agent (Default)",   // description
                     "Start the agent",             // help string
                     start_function);               // callback
}

HW::ParseResult parseArguments(const int argc, char* const argv[])
{
    // manipulate argc and v to make start the default action.
    // TODO(ploubser): Add ability to specify default action to HorseWhisperer
    int modified_argc = argc + 1;
    char* modified_argv[modified_argc];
    char action[] = "start";

    for (int i = 0; i < argc; i++) {
        modified_argv[i] = argv[i];
    }
    modified_argv[modified_argc - 1] = action;

    return HW::Parse(modified_argc, modified_argv);
}

HW::ParseResult Configuration::parseOptions(int argc, char *argv[])
{
    // If parsing options, clear the previous value for primary_uris.
    // This is primarily for testing.
    primary_uris_.clear();

    // Remember the path to the pxp-agent executable used to start
    // this process, it is supposed to be used to start executables
    // which are installed alongside the pxp-agent executable.
    exec_prefix_ = fs::path(argv[0]).parent_path();
    // If a relative or absolute path was specified, convert to absolute.
    // Otherwise, we'll depend on using PATH to find the execution_wrapper.
    if (!exec_prefix_.empty()) {
        exec_prefix_ = fs::absolute(exec_prefix_);
    }

    auto parse_result = parseArguments(argc, argv);

    if (parse_result == HW::ParseResult::FAILURE
        || parse_result == HW::ParseResult::INVALID_FLAG) {
        throw Configuration::Error {
            lth_loc::translate("An error occurred while parsing cli options") };
    }

    if (parse_result == HW::ParseResult::OK) {
        config_file_ = lth_file::tilde_expand(HW::GetFlag<std::string>("config-file"));
        if (!config_file_.empty()) {
            parseConfigFile();
        }
    }

    // Normalize and generate log dirs
    std::vector<std::string> options { "logfile" };
    if (HW::GetFlag<bool>("log-pcp-access")) {
        options.emplace_back("pcp-access-logfile");
    }

    for (const auto& option : options) {
        auto val = HW::GetFlag<std::string>(option);
        // Ignore not-a-directory options
        if (val == "-" || val == "eventlog" || val == "syslog")
            continue;

        fs::path val_path { lth_file::tilde_expand(val) };
        HW::SetFlag(option, val_path.string());
        auto dir = val_path.parent_path();
        check_and_create_dir(dir, option, true);
    }

    // No further processing or user interaction are required if
    // the parsing outcome is HW::ParseResult::HELP or VERSION
    return parse_result;
}

lth_log::log_level string_to_log_level(std::string loglevel)
{
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
        throw Configuration::Error {
            lth_loc::format("invalid log level: '{1}'", loglevel) };
    }
    return lvl;
}

std::string Configuration::setupLogging()
{
    logfile_ = HW::GetFlag<std::string>("logfile");
    auto loglevel = HW::GetFlag<std::string>("loglevel");

    if (logfile_ == "eventlog") {
      loglevel = Configuration::setupEventLogLogging(loglevel);
    } else if (logfile_ == "syslog") {
      loglevel = Configuration::setupSyslogLogging(loglevel);
    } else {
      loglevel = Configuration::setupFileLogging(loglevel);
    }
    return loglevel;
}

std::string Configuration::setupEventLogLogging(std::string loglevel)
{
#ifdef _WIN32

#ifdef HAS_LTH_EVENTLOG
    lth_log::setup_eventlog_logging("pxp-agent");

    lth_log::log_level lvl = string_to_log_level(loglevel);
    lth_log::set_level(lvl);
#else  // HAS_LTH_EVENTLOG
    throw Configuration::Error {
    lth_loc::format("Leatherman version does not support event log") };
#endif  // HAS_LTH_EVENTLOG

#else  // _WIN32
    throw Configuration::Error {
      lth_loc::format("eventlog is available only on windows") };
#endif  // _WIN32
    return loglevel;
}

std::string Configuration::setupSyslogLogging(std::string loglevel)
{
#ifdef _WIN32
    throw Configuration::Error {
      lth_loc::format("eventlog is available only on POSIX platforms") };
#else  // _WIN32
#ifdef HAS_LTH_SYSLOG
    const std::string facility = HW::GetFlag<std::string>("syslog-facility");

    lth_log::setup_syslog_logging(APPLICATION_NAME, facility);
    lth_log::log_level lvl = string_to_log_level(loglevel);
    lth_log::set_level(lvl);
#else  // HAS_LTH_SYSLOG
  throw Configuration::Error{
      lth_loc::format("Leatherman version does not support logging to syslog")};
#endif  // HAS_LTH_SYSLOG
#endif  // _WIN32
    return loglevel;
}

std::string Configuration::setupFileLogging(std::string loglevel)
{
    pcp_access_logfile_ = HW::GetFlag<std::string>("pcp-access-logfile");
    auto log_access = HW::GetFlag<bool>("log-pcp-access");
    auto log_on_stdout = (logfile_ == "-");
    std::ostream *log_stream = nullptr;

    if (!log_on_stdout) {
        // We should log on file
        logfile_fstream_.open(logfile_.c_str(), std::ios_base::app);
        fs::permissions(logfile_, NIX_FILE_PERMS);

        log_stream = &logfile_fstream_;
    } else {
        log_stream = &boost::nowide::cout;
    }

    if (log_access) {
        pcp_access_logfile_ = lth_file::tilde_expand(pcp_access_logfile_);
        pcp_access_fstream_ptr_.reset(
            new boost::nowide::ofstream(pcp_access_logfile_.c_str(),
                                        std::ios_base::app));
        fs::permissions(pcp_access_logfile_, NIX_FILE_PERMS);
    }

#ifndef _WIN32
    if (!HW::GetFlag<bool>("foreground") && log_on_stdout) {
        // NOTE(ale): util/posix/daemonize.cc will ensure that the
        // daemon is not associated with any controlling terminal.
        // It will also redirect stdout to /dev/null, together
        // with the other standard files. Set log_level to none to
        // reduce useless overhead since logfile is set to stdout.
        loglevel = "none";
    }
#endif

    lth_log::log_level lvl = string_to_log_level(loglevel);
    // Configure logging for pxp-agent
    lth_log::setup_logging(*log_stream);
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
    PCPClient::Util::setupLogging(*log_stream,
                                  force_colorization,
                                  loglevel,
                                  pcp_access_fstream_ptr_);

    if (!log_on_stdout) {
        // Configure platform-specific things for file logging
        // NB: we do that after setting up lth_log in order to log in
        //     case of failure
        configure_platform_file_logging();
    }

    return loglevel;
}

void Configuration::validate()
{
    validateAndNormalizeWebsocketSettings();
    validateAndNormalizeOtherSettings();
    valid_ = true;
}

const Configuration::Agent& Configuration::getAgentConfiguration() const
{
    assert(valid_);
    agent_configuration_ = Configuration::Agent {
        HW::GetFlag<std::string>("modules-dir"),
        broker_ws_uris_,
        primary_uris_,
        HW::GetFlag<std::string>("pcp-version"),
        HW::GetFlag<std::string>("ssl-ca-cert"),
        HW::GetFlag<std::string>("ssl-cert"),
        HW::GetFlag<std::string>("ssl-key"),
        HW::GetFlag<std::string>("ssl-crl"),
        HW::GetFlag<std::string>("spool-dir"),
        HW::GetFlag<std::string>("spool-dir-purge-ttl"),
        HW::GetFlag<std::string>("modules-config-dir"),
        HW::GetFlag<std::string>("task-cache-dir"),
        HW::GetFlag<std::string>("task-cache-dir-purge-ttl"),
        AGENT_CLIENT_TYPE,
        HW::GetFlag<std::string>("broker-ws-proxy"),
        HW::GetFlag<std::string>("master-proxy"),
        HW::GetFlag<int>("connection-timeout") * 1000,
        static_cast<uint32_t >(HW::GetFlag<int>("association-timeout")),
        static_cast<uint32_t >(HW::GetFlag<int>("association-request-ttl")),
        static_cast<uint32_t >(HW::GetFlag<int>("pcp-message-ttl")),
        static_cast<uint32_t >(HW::GetFlag<int>("allowed-keepalive-timeouts")),
        static_cast<uint32_t >(HW::GetFlag<int>("ping-interval")),
        static_cast<uint32_t >(HW::GetFlag<int>("task-download-connect-timeout")),
        static_cast<uint32_t >(HW::GetFlag<int>("task-download-timeout")),
        HW::GetFlag<uint32_t>("max-message-size"),
        string_to_log_level(HW::GetFlag<std::string>("loglevel")) };
    return agent_configuration_;
}

void Configuration::reopenLogfiles() const
{
    if (!logfile_.empty() && logfile_ != "-") {
        LOG_INFO("Reopening the pxp-agent log file");

        try {
            logfile_fstream_.close();
        } catch (const std::exception& e) {
            LOG_DEBUG("Failed to close the pxp-agent log file stream; already closed?");
        }
        try {
            logfile_fstream_.open(logfile_.c_str(), std::ios_base::app);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to open the pxp-agent log file stream");
        }
    } else {
        LOG_DEBUG("pxp-agent logging was not configured; no file will be reopened");
    }

    if (HW::GetFlag<bool>("log-pcp-access")) {
        LOG_INFO("Reopening the PCP Access log file");

        try {
            pcp_access_fstream_ptr_->close();
        } catch (const std::exception& e) {
            LOG_DEBUG("Failed to close the PCP Access log file stream; already closed?");
        }
        try {
            pcp_access_fstream_ptr_->open(pcp_access_logfile_.c_str(), std::ios_base::app);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to open the PCP Access log file stream");
        }
    } else {
        LOG_DEBUG("PCP Access logging was not configured; no file will be reopened");
    }
}

//
// Private interface
//

Configuration::Configuration() : valid_ { false },
                                 defaults_ {},
                                 config_file_ { "" },
                                 agent_configuration_ {},
                                 logfile_ { "" },
                                 pcp_access_logfile_ { "" },
                                 logfile_fstream_ {},
                                 pcp_access_fstream_ptr_ { nullptr }
{
    defineDefaultValues();
}

void Configuration::defineDefaultValues()
{
    defaults_.insert(
        Option { "config-file",
                 Base_ptr { new Entry<std::string>(
                    "config-file",
                    "",
                    lth_loc::format("Config file, default: {1}", DEFAULT_CONFIG_FILE),
                    Types::String,
                    DEFAULT_CONFIG_FILE) } });

    defaults_.insert(
        Option { "broker-ws-uri",
                 Base_ptr { new Entry<std::string>(
                    "broker-ws-uri",
                    "",
                    lth_loc::translate("WebSocket URI of the PCP broker"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "master-uri",
                 Base_ptr { new Entry<std::string>(
                    "master-uri",
                    "",
                    lth_loc::translate("URI of the Puppet Primary"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "primary-uri",
                 Base_ptr { new Entry<std::string>(
                    "primary-uri",
                    "",
                    lth_loc::translate("URI of the Puppet Primary"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "pcp-version",
                 Base_ptr { new Entry<std::string>(
                    "pcp-version",
                    "",
                    lth_loc::translate("PCP version to use for communication"),
                    Types::String,
                    DEFAULT_PCP_VERSION) } });

    defaults_.insert(
        Option { "connection-timeout",
                 Base_ptr { new Entry<int>(
                    "connection-timeout",
                    "",
                    lth_loc::translate("Timeout (in seconds) for starting the "
                                       "WebSocket handshake, default: 5 s"),
                    Types::Int,
                    5) } });

    // Hidden option: number of ping/pong timeouts to allow before
    // disconnecting, default: 2
    defaults_.insert(
        Option { "allowed-keepalive-timeouts",
                 Base_ptr { new Entry<int>(
                    "allowed-keepalive-timeouts",
                    "",
                    "<hidden>",
                    Types::Int,
                    2) } });

    // Hidden option: interval between pings, default 120 s
    defaults_.insert(
        Option { "ping-interval",
                 Base_ptr { new Entry<int>(
                    "ping-interval",
                    "",
                    "<hidden>",
                    Types::Int,
                    120) } });

    // Hidden option: PCP Association timeout, default: 15 s
    defaults_.insert(
        Option { "association-timeout",
                 Base_ptr { new Entry<int>(
                    "association-timeout",
                    "",
                    "<hidden>",
                    Types::Int,
                    15) } });

    // Hidden option: TTL of the PCP Association request, default: 10 s
    defaults_.insert(
            Option { "association-request-ttl",
                     Base_ptr { new Entry<int>(
                             "association-request-ttl",
                             "",
                             "<hidden>",
                             Types::Int,
                             10) } });

    // Hidden option: TTL of the PCP messages, default: 5 s
    defaults_.insert(
        Option { "pcp-message-ttl",
                 Base_ptr { new Entry<int>(
                    "pcp-message-ttl",
                    "",
                    "<hidden>",
                    Types::Int,
                    5) } });

    defaults_.insert(
        Option { "task-download-connect-timeout",
                 Base_ptr { new Entry<int>(
                    "task-download-connect-timeout",
                    "",
                    lth_loc::translate("Connection timeout when downloading tasks, default: 120 s"),
                    Types::Int,
                    2*60) } });

    defaults_.insert(
        Option { "task-download-timeout",
                 Base_ptr { new Entry<int>(
                    "task-download-timeout",
                    "",
                    lth_loc::translate("Total timeout when downloading tasks, default: 1800 s"),
                    Types::Int,
                    30*60) } });

    defaults_.insert(
        Option { "ssl-ca-cert",
                 Base_ptr { new Entry<std::string>(
                    "ssl-ca-cert",
                    "",
                    lth_loc::translate("SSL CA certificate"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-cert",
                 Base_ptr { new Entry<std::string>(
                    "ssl-cert",
                    "",
                    lth_loc::translate("pxp-agent SSL certificate"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-key",
                 Base_ptr { new Entry<std::string>(
                    "ssl-key",
                    "",
                    lth_loc::translate("pxp-agent private SSL key"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "ssl-crl",
                 Base_ptr { new Entry<std::string>(
                    "ssl-crl",
                    "",
                    lth_loc::translate("pxp-agent SSL certificate revocation list"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "logfile",
                 Base_ptr { new Entry<std::string>(
                    "logfile",
                    "",
                    lth_loc::format("Log file, default: {1}", DEFAULT_LOG_FILE),
                    Types::String,
                    DEFAULT_LOG_FILE) } });

#ifdef HAS_LTH_SYSLOG
    defaults_.insert(
        Option { "syslog-facility",
                 Base_ptr { new Entry<std::string>(
                    "syslog-facility",
                    "",
                    lth_loc::translate("Syslog facility to log to. Defaults to 'local5'"),
                    Types::String,
                    "local5") } });
#endif

    defaults_.insert(
        Option { "loglevel",
                 Base_ptr { new Entry<std::string>(
                    "loglevel",
                    "",
                    lth_loc::translate("Valid options are 'none', 'trace', "
                                       "'debug', 'info','warning', 'error' and "
                                       "'fatal'. Defaults to 'info'"),
                    Types::String,
                    "info") } });

    defaults_.insert(
            Option { "log-pcp-access",
                     Base_ptr { new Entry<bool>(
                             "log-pcp-access",
                             "",
                             lth_loc::translate("Enable PCP Access logging, default: false"),
                             Types::Bool,
                             false) } });

    defaults_.insert(
            Option { "pcp-access-logfile",
                     Base_ptr { new Entry<std::string>(
                             "pcp-access-logfile",
                             "",
                             lth_loc::format("PCP Access log file, default: {1}",
                                             DEFAULT_PCP_ACCESS_FILE),
                             Types::String,
                             DEFAULT_PCP_ACCESS_FILE) } });

    defaults_.insert(
        Option { "modules-dir",
                 Base_ptr { new Entry<std::string>(
                    "modules-dir",
                    "",
                    lth_loc::format("Modules directory, default"
#ifdef _WIN32
                                    " (relative to the pxp-agent.exe path)"
#endif
                                    ": {1}", DEFAULT_MODULES_DIR),
                    Types::String,
                    DEFAULT_MODULES_DIR) } });

    defaults_.insert(
        Option { "modules-config-dir",
                 Base_ptr { new Entry<std::string>(
                    "modules-config-dir",
                    "",
                    lth_loc::format("Module config files directory, default: {1}",
                                    DEFAULT_MODULES_CONF_DIR),
                    Types::String,
                    DEFAULT_MODULES_CONF_DIR) } });

    defaults_.insert(
        Option { "task-cache-dir",
                 Base_ptr { new Entry<std::string>(
                    "task-cache-dir",
                    "",
                    lth_loc::format("Tasks cache directory, default: {1}",
                                    DEFAULT_TASK_CACHE_DIR),
                    Types::String,
                    DEFAULT_TASK_CACHE_DIR) } });

    defaults_.insert(
        Option { "spool-dir",
                 Base_ptr { new Entry<std::string>(
                    "spool-dir",
                    "",
                    lth_loc::format("Spool action results directory, default: {1}",
                                    DEFAULT_SPOOL_DIR),
                    Types::String,
                    DEFAULT_SPOOL_DIR) } });

    defaults_.insert(
        Option { "spool-dir-purge-ttl",
                 Base_ptr { new Entry<std::string>(
                    "spool-dir-purge-ttl",
                    "",
                    lth_loc::format("TTL for action results before being deleted, "
                                    "default: '{1}' (days)",
                                    DEFAULT_DIR_PURGE_TTL),
                    Types::String,
                    DEFAULT_DIR_PURGE_TTL) } });

    defaults_.insert(
        Option { "task-cache-dir-purge-ttl",
                 Base_ptr { new Entry<std::string>(
                    "task-cache-dir-purge-ttl",
                    "",
                    lth_loc::format("TTL for cached tasks before being deleted, "
                                    "default: '{1}' (days)",
                                    DEFAULT_DIR_PURGE_TTL),
                    Types::String,
                    DEFAULT_DIR_PURGE_TTL) } });

    defaults_.insert(
        Option { "broker-ws-proxy",
                 Base_ptr { new Entry<std::string>(
                    "broker-ws-proxy",
                    "",
                    lth_loc::translate("PCP-broker WebSocket proxy"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "master-proxy",
                 Base_ptr { new Entry<std::string>(
                    "master-proxy",
                    "",
                    lth_loc::translate("Puppet Master proxy"),
                    Types::String,
                    "") } });

    defaults_.insert(
        Option { "foreground",
                 Base_ptr { new Entry<bool>(
                    "foreground",
                    "",
                    lth_loc::translate("Don't daemonize, default: false"),
                    Types::Bool,
                    false) } });

    defaults_.insert(
        Option { "max-message-size",
                 Base_ptr { new Entry<int>(
                    "max-message-size",
                    "",
                    lth_loc::translate("Maximum size in Bytes of messages to send to pcp-broker"),
                    Types::Int,
                    64 * 1024 * 1024) } });

#ifndef _WIN32
    // NOTE(ale): we don't daemonize on Windows; we rely NSSM to start
    // the pxp-agent service and on CreateMutexA() to avoid multiple
    // pxp-agent instances at once
    defaults_.insert(
        Option { "pidfile",
                 Base_ptr { new Entry<std::string>(
                    "pidfile",
                    "",
                    lth_loc::format("PID file path, default: {1}", DEFAULT_PID_FILE),
                    Types::String,
                    DEFAULT_PID_FILE) } });
#endif
}

void Configuration::setDefaultValues()
{
    for (auto opt_idx = defaults_.get<Option::ByInsertion>().begin();
         opt_idx != defaults_.get<Option::ByInsertion>().end();
         ++opt_idx) {
        opt_idx->ptr->configured = false;
        std::string flag_names { opt_idx->ptr->name };

        if (!opt_idx->ptr->aliases.empty()) {
            flag_names += " " + opt_idx->ptr->aliases;
        }

        switch (opt_idx->ptr->type) {
            case Types::Int:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<int>(flag_names, entry_ptr->help,
                                              entry_ptr->value,
                                              [entry_ptr] (int v) {
                                                  entry_ptr->configured = true;
                                              });
                }
                break;
            case Types::Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<bool>(flag_names, entry_ptr->help,
                                               entry_ptr->value,
                                               [entry_ptr] (bool v) {
                                                   entry_ptr->configured = true;
                                               });
                }
                break;
            case Types::Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<double>(flag_names, entry_ptr->help,
                                                 entry_ptr->value,
                                                 [entry_ptr] (double v) {
                                                     entry_ptr->configured = true;
                                                 });
                }
                break;
            case Types::String:
                {
                    Entry<std::string>* entry_ptr = (Entry<std::string>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<std::string>(flag_names, entry_ptr->help,
                                                      entry_ptr->value,
                                                      [entry_ptr] (std::string v) {
                                                          entry_ptr->configured = true;
                                                      });
                }
                break;
            case Types::MultiString:
                {
                    Entry<std::vector<std::string>>* entry_ptr = (Entry<std::vector<std::string>>*) opt_idx->ptr.get();
                    HW::DefineGlobalFlag<std::vector<std::string>>(flag_names, entry_ptr->help,
                                                      entry_ptr->value,
                                                      [entry_ptr] (std::vector<std::string> v) {
                                                          entry_ptr->configured = true;
                                                      });
                }
                break;
            default:
                // Present because FlagType is not an enum class, and I don't trust
                // compilers to warn/error about missing cases.
                assert(false);
        }
    }
}

void Configuration::parseConfigFile()
{
    if (!lth_file::file_readable(config_file_)) {
        LOG_DEBUG("Config file {1} absent", config_file_);
        return;
    }

    using boost::algorithm::ends_with;
    if (!ends_with(config_file_, ".conf") && !ends_with(config_file_, ".json")) {
        // This is required by the HOCON parser.
        throw Configuration::Error {
            lth_loc::format("config-file {1} must end with .conf or .json", config_file_) };
    }

    hocon::shared_config config;
    try {
        LOG_TRACE("Parsing {1}", config_file_);
        config = hocon::config::parse_file_any_syntax(config_file_)->resolve();
    } catch (hocon::config_exception& e) {
        throw Configuration::Error {
            lth_loc::format("cannot parse config file: {1}", e.what()) };
    }

    auto wrap =
        [] (const std::string& key,
            const std::string& type_str,
            std::function<void()> assignment) {
            try {
                assignment();
            } catch (hocon::config_exception& e) {
                throw Configuration::Error { lth_loc::format("field '{1}' must be of type {2}", key, type_str) };
            }
        };

    bool broker_ws_uri_in_file = false;

    LOG_TRACE("Iterating config: {1}", config->root()->render());
    for (const auto& entry : config->entry_set()) {
        auto key = entry.first;
        LOG_TRACE("Key = {1}", key);
        const auto& opt_idx = defaults_.get<Option::ByName>().find(key);

        if (opt_idx == defaults_.get<Option::ByName>().end()) {
            // broker-ws-uris is restricted to the config file, so it's handled
            // specially here. It's not part of defaults_ because all the extra
            // meta-data for Horsewhisperer isn't needed.
            // TODO: add `servers`, which will populate broker-ws-uris and primary-uris
            //       using default ports; expects hostnames only
            if (key == "broker-ws-uris") {
                wrap(key, lth_loc::translate("array of strings"), [&]() {
                    broker_ws_uris_ = config->get_string_list(key);
                });
                continue;
            } else if (key == "primary-uris") {
                wrap(key, lth_loc::translate("array of strings"), [&]() {
                    primary_uris_ = config->get_string_list(key);
                });
                continue;
            } else if (key == "master-uris" && primary_uris_.size() == 0) {
                wrap(key, lth_loc::translate("array of strings"), [&]() {
                    primary_uris_ = config->get_string_list(key);
                });
                continue;
            } else {
                LOG_INFO("Ignoring unrecognized option '{1}'", key);
                continue;
            }
        }

        if (opt_idx->ptr->configured)
            continue;

        switch (opt_idx->ptr->type) {
            case Types::Int:
                wrap(key, lth_loc::translate("integer"), [&]() {
                    HW::SetFlag(key, config->get_int(key));
                });
                break;
            case Types::Bool:
                wrap(key, lth_loc::translate("boolean"), [&]() {
                    HW::SetFlag(key, config->get_bool(key));
                });
                break;
            case Types::Double:
                wrap(key, lth_loc::translate("double"), [&]() {
                    HW::SetFlag(key, config->get_double(key));
                });
                break;
            case Types::String:
                if (key == "broker-ws-uri") {
                    broker_ws_uri_in_file = true;
                }
                wrap(key, lth_loc::translate("string"), [&]() {
                    HW::SetFlag(key, config->get_string(key));
                });
                break;
            default:
                // Present because FlagType is not an enum class, and I don't trust
                // compilers to warn/error about missing cases.
                assert(false);
        }
    }

    if (!broker_ws_uris_.empty() && broker_ws_uri_in_file) {
        throw Configuration::Error {
            lth_loc::format("broker-ws-uri and broker-ws-uris cannot both be "
                            "defined in the config file") };
    }
}

// Helper function
std::string check_and_expand_ssl_cert(const std::string& cert_name)
{
    auto c = HW::GetFlag<std::string>(cert_name);
    if (c.empty())
        throw Configuration::Error {
            lth_loc::format("{1} value must be defined", cert_name) };

    c = lth_file::tilde_expand(c);
    if (!fs::exists(c))
        throw Configuration::Error {
            lth_loc::format("{1} file '{2}' not found", cert_name, c) };

    if (!lth_file::file_readable(c))
        throw Configuration::Error {
            lth_loc::format("{1} file '{2}' not readable", cert_name, c) };

    return c;
}

std::string check_and_expand_ssl_cert_if_not_empty(const std::string& cert_name)
{
    auto c = HW::GetFlag<std::string>(cert_name);
    if (c.empty())
        return c;

    c = lth_file::tilde_expand(c);
    if (!fs::exists(c))
        throw Configuration::Error {
            lth_loc::format("{1} file '{2}' not found", cert_name, c) };

    if (!lth_file::file_readable(c))
        throw Configuration::Error {
            lth_loc::format("{1} file '{2}' not readable", cert_name, c) };

    return c;
}

static void validate_wss(std::string const& uri, std::string const& name)
{
    if (uri.compare(0, 6, "wss://") != 0)
        throw Configuration::Error {
            lth_loc::format("{1} value \"{2}\" must start with wss://", name, uri) };
}

void Configuration::validateAndNormalizeWebsocketSettings()
{
    // Check the broker's WebSocket URI
    // broker-ws-uri and broker-ws-uris cannot both be configured in the config
    // file (enforced in parseConfigFile). If broker-ws-uri is set on the CLI,
    // override whatever was in the config file.
    auto broker_ws_uri = HW::GetFlag<std::string>("broker-ws-uri");
    if (!broker_ws_uri.empty()) {
        validate_wss(broker_ws_uri, "broker-ws-uri");
        broker_ws_uris_.clear();
        broker_ws_uris_.push_back(std::move(broker_ws_uri));
    } else {
        for (auto const& uri : broker_ws_uris_) {
            validate_wss(uri, "broker-ws-uris");
        }
    }

    for (auto &uri : primary_uris_) {
        auto parsed = lth_util::uri(uri);
        if (parsed.protocol.empty()) {
            parsed.protocol = "https";
        } else if (parsed.protocol != "https") {
            throw Configuration::Error {
                lth_loc::format("primary-uris value \"{1}\" must start with https://", uri) };
        }

        if (parsed.port.empty()) {
            parsed.port = "8140";
        }
        uri = parsed.str();
    }

    if (broker_ws_uris_.empty())
        throw Configuration::Error {
            lth_loc::translate("broker-ws-uri or broker-ws-uris must be defined") };

    auto pcp_version = HW::GetFlag<std::string>("pcp-version");
    if (pcp_version != "1" && pcp_version != "2") {
        throw Configuration::Error {
            lth_loc::translate("pcp-version must be either '1' or '2'") };
    }

    // Check the SSL options and expand the paths
    auto ca   = check_and_expand_ssl_cert("ssl-ca-cert");
    auto cert = check_and_expand_ssl_cert("ssl-cert");
    auto key  = check_and_expand_ssl_cert("ssl-key");
    auto crl  = check_and_expand_ssl_cert_if_not_empty("ssl-crl");

    // Ensure client certs are good
    try {
        PCPClient::getCommonNameFromCert(cert);
        PCPClient::validatePrivateKeyCertPair(key, cert);
    } catch (const PCPClient::connection_config_error& e) {
        throw Configuration::Error { e.what() };
    }

    // Set the expanded cert paths
    HW::SetFlag<std::string>("ssl-ca-cert", ca);
    HW::SetFlag<std::string>("ssl-cert", cert);
    HW::SetFlag<std::string>("ssl-key", key);
    HW::SetFlag<std::string>("ssl-crl", crl);
}

void Configuration::validateAndNormalizeOtherSettings()
{
    // Normalize and check directories. The modules directory
    // is expected to exist, and will usually come from packaging.
    std::vector<std::pair<std::string, bool>> options {
        std::make_pair(std::string("modules-dir"), false),
        std::make_pair(std::string("modules-config-dir"), true),
        std::make_pair(std::string("task-cache-dir"), true),
        std::make_pair(std::string("spool-dir"), true) };

    for (const auto& option : options) {
        auto val = HW::GetFlag<std::string>(option.first);
        fs::path val_path { lth_file::tilde_expand(val) };
        HW::SetFlag(option.first, val_path.string());
        check_and_create_dir(val_path, option.first, option.second);
    }

    fs::path spool_dir_path = HW::GetFlag<std::string>("spool-dir");
    fs::path tmp_path;
    do {
        tmp_path = spool_dir_path / lth_util::get_UUID();
    } while (fs::exists(tmp_path));

    bool is_writable { false };
    try {
        if (fs::create_directories(tmp_path)) {
            is_writable = true;
            fs::remove_all(tmp_path);
        }
    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to create the test dir in spool path '{1}' during"
                  "configuration validation: {2}",
                  spool_dir_path.string(), e.what());
    }

    if (!is_writable)
        throw Configuration::Error {
            lth_loc::format("the spool-dir '{1}' is not writable",
                            spool_dir_path.string()) };

#ifndef _WIN32
    if (!HW::GetFlag<bool>("foreground")) {
        auto pid_file = lth_file::tilde_expand(HW::GetFlag<std::string>("pidfile"));

        if (fs::exists(pid_file) && !(fs::is_regular_file(pid_file)))
            throw Configuration::Error {
                lth_loc::format("the PID file '{1}' is not a regular file", pid_file) };

        auto pid_dir = fs::path(pid_file).parent_path().string();
        HW::SetFlag<std::string>("pidfile", pid_file);
        check_and_create_dir(pid_dir, "pidfile", true);
    }
#endif

    for (auto purge_ttl : {"spool-dir-purge-ttl", "task-cache-dir-purge-ttl"}) {
        try {
            Timestamp(HW::GetFlag<std::string>(purge_ttl));
        } catch (const Timestamp::Error& e) {
            throw Configuration::Error {
                // LOCALE: invalid configuration option
                lth_loc::format("invalid {1}: {2}", purge_ttl, e.what()) };
        }
    }

    for (auto msg_ttl : {"association-timeout",
                         "association-request-ttl",
                         "pcp-message-ttl",
                         "task-download-connect-timeout",
                         "task-download-timeout"}) {
        if (HW::GetFlag<int>(msg_ttl) < 0)
            throw Configuration::Error {
                lth_loc::format("{1} must be positive", msg_ttl) };
    }
}

const Options::iterator Configuration::getDefaultIndex(const std::string& flagname)
{
    const auto& opt_idx = defaults_.get<Option::ByName>().find(flagname);
    if (opt_idx == defaults_.get<Option::ByName>().end())
        throw Configuration::Error {
            lth_loc::format("no default value for {1}", flagname) };
    return opt_idx;
}

std::string Configuration::getInvalidFlagError(const std::string& flagname)
{
    return lth_loc::format("invalid value for {1}", flagname);
}

std::string Configuration::getUnknownFlagError(const std::string& flagname)
{
    return lth_loc::format("unknown flag name: {1}", flagname);
}

void Configuration::checkValidForSetting()
{
    if (!valid_)
        throw Configuration::Error {
            lth_loc::translate("cannot set configuration value until "
                               "configuration is validated") };
}

}  // namespace PXPAgent
