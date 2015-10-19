#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include <horsewhisperer/horsewhisperer.h>

#include <boost/nowide/fstream.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <memory>
#include <stdexcept>

namespace PXPAgent {

namespace HW = HorseWhisperer;

//
// Tokens
//

extern const std::string DEFAULT_SPOOL_DIR;     // used by unit tests
extern const std::string LOGFILE_NAME;          // not configurable
extern const std::string PID_DIR;               // not configurable

//
// Types
//

enum Types { Integer, String, Bool, Double };

struct EntryBase {
    // Config option name
    // HERE: must match one of the flag names and config file option
    std::string name;
    // CLI option aliases (e.g. --server --serv -s)
    std::string aliases;
    // Help string to be displayed by --help flag
    std::string help;
    // Value type
    Types type;
    // Whether the option was already parsed from config file
    bool configured = false;

    EntryBase(std::string _name, std::string _aliases, std::string _help, Types _type)
            : name { _name },
              aliases { _aliases },
              help { _help },
              type { _type } {
    }
};

template <typename T>
struct Entry : EntryBase {
    // Default value
    T value;

    Entry<T>(std::string _name, std::string _aliases, std::string _help, Types _type,
             T _value)
            : EntryBase { _name, _aliases, _help, _type },
              value { _value } {
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
// Configuration
//

class Configuration {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    struct UnconfiguredError : public Error {
        explicit UnconfiguredError(std::string const& msg) : Error(msg) {}
    };

    static Configuration& Instance() {
        static Configuration instance {};
        return instance;
    }

    struct Agent {
        std::string modules_dir;
        std::string server_url;
        std::string ca;
        std::string crt;
        std::string key;
        std::string spool_dir;
        std::string modules_config_dir;
        std::string client_type;
    };

    /// Set the configuration entries to their default values.
    /// Unset the initialized_ flag.
    void reset();

    /// Define the default configuration values that depend on the
    /// location of the executable.
    /// Parse the command line arguments and, if specified, the
    /// configuration file, in order to obtain the configuration
    /// values; validate and normalize them.
    /// Enable logging if flagged (flag useful for prevent logging
    /// during testing).
    /// Return a HorseWhisperer::ParseResult value indicating the
    /// parsing outcome (refer to HorseWhisperer).
    /// Throw a HorseWhisperer::flag_validation_error in case such
    /// exception is thrown by a flag validation callback.
    /// Throw a Configuration::Error: if it fails to parse the CLI
    /// arguments; in case of a missing or invalid configuration
    /// value; if the specified config file cannot be parsed or has
    /// any invalid JSON entry.
    HW::ParseResult initialize(int argc, char *argv[],
                               bool enable_logging = true);

    /// Set the start function that will be executed when calling
    /// HorseWhisperer::Start().
    void setStartFunction(std::function<int(std::vector<std::string>)> start_function);

    /// If Configuration was already initialized, return the value
    /// set for the specified flag (NB: the value can be a default);
    /// throw a Configuration::Error such flag is unknown.
    /// If Configuration was not initialized so far, but the
    /// requested flag is known, return the default value; throw a
    /// Configuration::Error otherwise.
    template <typename T>
    T get(std::string flagname) {
        if (initialized_) {
            try {
                return HW::GetFlag<T>(flagname);
            } catch (HW::undefined_flag_error& e) {
                throw Configuration::Error { std::string { e.what() } };
            }
        }

        // Configuration instance not initialized; get default
        const auto& opt_idx = defaults_.get<Option::ByName>().find(flagname);

        if (opt_idx == defaults_.get<Option::ByName>().end()) {
            throw Configuration::Error { "no default value for " + flagname };
        } else {
            Entry<T>* entry_ptr = (Entry<T>*) opt_idx->ptr.get();
            return entry_ptr->value;
        }
    }

    /// Set the specified value for a given configuration flag.
    /// Throw an Configuration::Error in case the Configuration was
    /// not initialized so far.
    /// Throw a Configuration::Error in case the specified flag is
    /// unknown or the value is invalid.
    template<typename T>
    void set(std::string flagname, T value) {
        if (!initialized_) {
            throw Configuration::Error { "cannot set configuration value until "
                                         "initialized" };
        }

        try {
            HW::SetFlag<T>(flagname, value);
        } catch (HW::flag_validation_error) {
            throw Configuration::Error { "invalid value for " + flagname };
        } catch (HW::undefined_flag_error) {
            throw Configuration::Error { "unknown flag name: " + flagname };
        }
    }

    /// Ensure all required values are valid. If necessary, expand
    /// file paths to the expected format.
    /// Throw a Configuration::Error if any required file path is
    /// missing or in case of invalid values.
    void validateAndNormalizeConfiguration();

    /// Return the whole agent configuration
    const Agent& getAgentConfiguration() const;

    /// Returns true if the agent was successfuly initialized
    bool isInitialized() const;

    /// Try to close the log file stream,  then try to open the log
    /// file in append mode and associate it to the log file stream.
    /// All possible exceptions will be filtered.
    void reopenLogfile() const;

  private:
    // Whether Configuration singleton was successfully instantiated
    bool initialized_;

    // Stores options with relative default values
    Options defaults_;

    // Path to the pxp-agent configuration file
    std::string config_file_;

    // Function that starts the pxp-agent service
    std::function<int(std::vector<std::string>)> start_function_;

    // Cache agent configuration parameters
    Agent agent_configuration_;

    // Path to the logfile
    std::string logfile_;

    // Stream abstraction object for the logfile
    mutable boost::nowide::ofstream logfile_fstream_;

    Configuration();
    void defineDefaultValues();
    void setDefaultValues();
    void parseConfigFile();
    void setupLogging();
    void setAgentConfiguration();
};

}  // namespace PXPAgent

#endif  // SRC_CONFIGURATION_H_
