#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include <horsewhisperer/horsewhisperer.h>

#include <boost/filesystem.hpp>

#include <boost/nowide/fstream.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <memory>
#include <stdexcept>
#include <cstdint>

namespace PXPAgent {

//
// Tokens
//

extern const std::string DEFAULT_SPOOL_DIR;     // used by unit tests

// Note that on Windows we rely on inherited directory ACLs.
extern const boost::filesystem::perms NIX_FILE_PERMS;
extern const boost::filesystem::perms NIX_DIR_PERMS;

//
// Types
//

using Types = HorseWhisperer::FlagType;

struct EntryBase
{
    // Config option name
    // HERE: must match one of the flag names and config file option
    std::string name;
    // CLI option aliases (e.g. --enable-transmogrification -t)
    std::string aliases;
    // Help string to be displayed by --help flag
    std::string help;
    // Value type
    Types type;
    // Whether the option was already parsed from config file
    bool configured = false;

    EntryBase(std::string _name, std::string _aliases, std::string _help, Types _type)
            : name { std::move(_name) },
              aliases { std::move(_aliases) },
              help { std::move(_help) },
              type { std::move(_type) } {
    }

    virtual ~EntryBase() = default;
};

template <typename T>
struct Entry : EntryBase
{
    // Default value
    T value;

    Entry<T>(std::string _name, std::string _aliases, std::string _help, Types _type,
             T _value)
            : EntryBase { std::move(_name), std::move(_aliases), std::move(_help), std::move(_type) },
              value { std::move(_value) } {
    }
};

using Base_ptr = std::unique_ptr<EntryBase>;

// Use a boost::multi_index container to allow accessing default
// options by key (option name) and insertion order
struct Option {
    std::string name;
    Base_ptr ptr;

    // boost::multi_index tag: retrieve option by name, map-style
    // NB: any type can be used as a multi-index tag
    struct ByName {};

    // boost::multi_index tag: retrieve option by insertion order,
    // vector-style
    struct ByInsertion {};
};

typedef boost::multi_index::multi_index_container<
    Option,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<Option::ByName>,
            boost::multi_index::member<Option,
                                       std::string,
                                       &Option::name>
        >,
        boost::multi_index::random_access<
            boost::multi_index::tag<Option::ByInsertion>
        >
    >
> Options;

//
// Platform-specific interface
//

/// Perform the platform specific configuration steps for setting up
/// the pxp-agent logging to file.
/// Throw a Configuration::Error in case of failure.
void configure_platform_file_logging();

//
// Configuration (singleton)
//

class Configuration
{
  public:
    struct Error : public std::runtime_error
    {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    static Configuration& Instance()
    {
        static Configuration instance {};
        return instance;
    }

    struct Agent
    {
        std::string modules_dir;
        std::vector<std::string> broker_ws_uris;
        std::vector<std::string> master_uris;
        std::string pcp_version;
        std::string ca;
        std::string crt;
        std::string key;
        std::string crl;
        std::string spool_dir;
        std::string spool_dir_purge_ttl;
        std::string modules_config_dir;
        std::string task_cache_dir;
        std::string task_cache_dir_purge_ttl;
        std::string client_type;
        std::string broker_ws_proxy;
        std::string master_proxy;
        long ws_connection_timeout_ms;
        uint32_t association_timeout_s;
        uint32_t association_request_ttl_s;
        uint32_t pcp_message_ttl_s;
        uint32_t allowed_keepalive_timeouts;
        uint32_t ping_interval_s;
        uint32_t task_download_connect_timeout_s;
        uint32_t task_download_timeout_s;
        uint32_t max_message_size;
    };

    /// Reset the HorseWhisperer singleton.
    /// Set the configuration entries to their default values and the
    /// specified start function that will be configured as the
    /// unique HorseWhisperer action for pxp-agent.
    /// Initialize the boost filesystem locale.
    void initialize(std::function<int(std::vector<std::string>)> start_function);

    /// Parse the command line arguments and, if specified, the
    /// configuration file.
    /// Return a HorseWhisperer::ParseResult value indicating the
    /// parsing outcome (refer to HorseWhisperer).
    /// Throw a HorseWhisperer::flag_validation_error in case such
    /// exception is thrown by a flag validation callback.
    /// Throw a Configuration::Error: if it fails to parse the CLI
    /// arguments; if the specified config file cannot be parsed or
    /// has any invalid JSON entry.
    HorseWhisperer::ParseResult parseOptions(int argc, char *argv[]);

    /// Validate logging configuration options and enable logging.
    /// Throw a Configuration::Error: in case of invalid the specified
    /// log file is in a non-esixtent directory.
    /// Other execeptions are propagated.
    /// Returns the log level.
    std::string setupLogging();

    /// Ensure all required values are valid. If necessary, expand
    /// file paths to the expected format.
    /// Throw a Configuration::Error in case an option is set to an
    /// invalid value.
    void validate();

    /// If Configuration was already validated, return the value
    /// set for the specified flag (NB: the value can be a default);
    /// throw a Configuration::Error such flag is unknown.
    /// If Configuration was not validated so far, return the default
    /// value in case nd the requested flag is known or throw a
    /// Configuration::Error otherwise.
    template <typename T>
    T get(std::string flagname)
    {
        if (valid_) {
            try {
                return HorseWhisperer::GetFlag<T>(flagname);
            } catch (HorseWhisperer::undefined_flag_error& e) {
                throw Configuration::Error { std::string { e.what() } };
            }
        }


        const auto& opt_idx = getDefaultIndex(flagname);
        Entry<T>* entry_ptr = (Entry<T>*) opt_idx->ptr.get();
        return entry_ptr->value;
    }

    /// Return the list of configured brokers.
    /// If called after validate(), this will be overridden by
    /// broker-ws-uri if it was set.
    std::vector<std::string> get_broker_ws_uris()
    {
        return broker_ws_uris_;
    }

    /// Return the list of configured masters.
    std::vector<std::string> get_master_uris()
    {
        return master_uris_;
    }

    /// Set the specified value for a given configuration flag.
    /// Throw an Configuration::Error in case the Configuration was
    /// not initialized so far.
    /// Throw a Configuration::Error in case the specified flag is
    /// unknown or the value is invalid.
    template<typename T>
    void set(std::string flagname, T value)
    {
        checkValidForSetting();

        try {
            HorseWhisperer::SetFlag<T>(flagname, std::move(value));
        } catch (HorseWhisperer::flag_validation_error&) {
            throw Configuration::Error { getInvalidFlagError(flagname) };
        } catch (HorseWhisperer::undefined_flag_error&) {
            throw Configuration::Error { getUnknownFlagError(flagname) };
        }
    }

    /// Return an object containing all agent configuration options
    const Agent& getAgentConfiguration() const;

    // Get the path used to start this pxp-agent process, its intended use
    // is to start executables which are installed alongside the pxp-agent
    // executable
    boost::filesystem::path& getExecPrefix() {
        return exec_prefix_;
    }

    /// Try to close the log file streams,  then try to open the log
    /// files (pxp-agent app log and PCP access log) in append mode
    /// and  associate them to the relative log file streams.
    /// All possible exceptions will be filtered.
    void reopenLogfiles() const;

  private:
    // Whether the Configuration singleton has successfully validated
    // all options specified by both CLI and file
    bool valid_;

    // Stores options with relative default values
    Options defaults_;

    // Path to the pxp-agent configuration file
    std::string config_file_;

    // Function that starts the pxp-agent service
    std::function<int(std::vector<std::string>)> start_function_;

    // Cache for agent configuration parameters
    mutable Agent agent_configuration_;

    // List of brokers
    std::vector<std::string> broker_ws_uris_;

    // List of masters
    std::vector<std::string> master_uris_;

    // Path to the logfile
    std::string logfile_;

    // Path to the PCP Access file
    std::string pcp_access_logfile_;

    // Stream abstraction object for the logfile
    mutable boost::nowide::ofstream logfile_fstream_;

    // Stream abstraction object for the PCP Access logfile
    mutable std::shared_ptr<boost::nowide::ofstream> pcp_access_fstream_ptr_;

    // The path used to start this pxp-agent process, it is used to start
    // executables which are installed alongside the pxp-agent executable
    boost::filesystem::path exec_prefix_;

    // Defines the default values
    Configuration();

    void defineDefaultValues();
    void setDefaultValues();
    void setStartAction();
    void parseConfigFile();
    void validateAndNormalizeWebsocketSettings();
    void validateAndNormalizeOtherSettings();
    void setAgentConfiguration();
    const Options::iterator getDefaultIndex(const std::string& flagname);
    std::string getInvalidFlagError(const std::string& flagname);
    std::string getUnknownFlagError(const std::string& flagname);
    void checkValidForSetting();
};

}  // namespace PXPAgent

#endif  // SRC_CONFIGURATION_H_
