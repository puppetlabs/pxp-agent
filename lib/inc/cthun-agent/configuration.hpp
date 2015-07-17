#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include <horsewhisperer/horsewhisperer.h>
#include <leatherman/json_container/json_container.hpp>

#include <map>

namespace CthunAgent {

namespace HW = HorseWhisperer;
namespace lth_jc = leatherman::json_container;

static const std::string DEFAULT_ACTION_RESULTS_DIR = "/tmp/cthun-agent/";

enum Types { Integer, String, Bool, Double };

struct EntryBase {
    // config option name. must match one of the flag names and config file option
    std::string name;
    // cli flag aliases (e.g. --server --serv -s)
    std::string aliases;
    // help string to be displayed by --help flag
    std::string help;
    // Value type
    Types type;
    bool configured = false;
    EntryBase(std::string Name, std::string Aliases,
              std::string Help, Types Type) : name(Name),
                                              aliases(Aliases),
                                              help(Help),
                                              type(Type) {}
};

template <typename T>
struct Entry : EntryBase {
    Entry<T>(std::string Name, std::string Aliases,
             std::string Help, Types Type, T Value) : EntryBase(Name, Aliases, Help, Type),
                                                      value(Value) {}
    // default value (can be empty)
    T value;
};

using Base_ptr = std::unique_ptr<EntryBase>;

class Configuration {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    struct UninitializedError : public Error {
        explicit UninitializedError(std::string const& msg) : Error(msg) {}
    };

    struct ConfigurationEntryError : public Error {
        explicit ConfigurationEntryError(std::string const& msg) : Error(msg) {}
    };

    struct ConfigfileError : public Error {
        explicit ConfigfileError(std::string const& msg) : Error(msg) {}
    };

    struct RequiredNotSetError : public Error {
        explicit RequiredNotSetError(std::string const& msg) : Error(msg) {}
    };

    struct CLIParseError : public Error {
        explicit CLIParseError(std::string const& msg) : Error(msg) {}
    };

    static Configuration& Instance() {
        static Configuration instance {};
        return instance;
    }

    /// Set the configuration entries to their default values.
    /// Unset the initialized_ flag.
    void reset();

    /// Define the default configuration values that depend on the
    /// location of the executable.
    /// Parse the command line arguments and, if specified, the
    /// configuration file, in order to obtain the configuration
    /// values; validate and normalize them.
    /// Return a HorseWhisperer::ParseResult value indicating the
    /// parsing outcome (refer to HorseWhisperer).
    /// Throw a HorseWhisperer::flag_validation_error in case such
    /// exception is thrown by a flag validation callback.
    /// Throw a Configuration::CLIParseError in case it fails to parse
    /// the CLI arguments.
    /// Throw a Configuration::RequiredNotSetError in case of a
    /// missing or invalid configuration value.
    /// Throw a Configuration::ConfigfileError if the specified config
    /// file cannot be parsed or has any invalid JSON entry.
    HW::ParseResult initialize(int argc, char *argv[]);

    /// Set the start function that will be executed when calling
    /// HorseWhisperer::Start().
    void setStartFunction(std::function<int(std::vector<std::string>)> start_function);

    /// If Configuration was already initialized, return the value
    /// set for the specified flag (NB: the value can be a default);
    /// throw a Configuration::ConfigurationEntryError such flag is
    /// unknown. If Configuration was not initialized so far, but the
    /// requested flag is known, return the default value; throw a
    /// Configuration::ConfigurationEntryError otherwise.
    template <typename T>
    T get(std::string flagname) {
        if (initialized_) {
            try {
                return HW::GetFlag<T>(flagname);
            } catch (HW::undefined_flag_error& e) {
                throw Configuration::ConfigurationEntryError {
                    std::string { e.what() } };
            }
        }

        // Configuration instance not initialized; get default
        auto entry = defaults_.find(flagname);
        if (entry == defaults_.end()) {
            throw Configuration::ConfigurationEntryError { "no default value for "
                                                           + flagname };
        } else {
            Entry<T>* entry_ptr = (Entry<T>*) entry->second.get();
            return entry_ptr->value;
        }
    }

    /// Set the specified value for a given configuration flag.
    /// Throw an Configuration::UninitializedError in case the
    /// Configuration was not initialized so far.
    /// Throw a Configuration::ConfigurationEntryError in case the
    /// specified flag is unknown or the value is invalid.
    template<typename T>
    void set(std::string flagname, T value) {
        if (!initialized_) {
            throw Configuration::UninitializedError { "cannot set configuration "
                                                      "value until initialized" };
        }

        try {
            HW::SetFlag<T>(flagname, value);
        } catch (HW::flag_validation_error) {
            throw Configuration::ConfigurationEntryError { "invalid value for "
                                                           + flagname };
        } catch (HW::undefined_flag_error) {
            throw Configuration::ConfigurationEntryError { "unknown flag name: "
                                                           + flagname };
        }
    }

    /// Ensure all required values are valid. If necessary, expand
    /// file paths to the expected format.
    /// Throw a Configuration::RequiredNotSetError if any required
    /// file path is missing; a Configuration::ConfigurationEntryError
    /// in case of invalid values.
    void validateAndNormalizeConfiguration();

    /// Load and store all configuration options for individual modules
    void loadModuleConfiguration();

    /// Retrieve module specific config options
    const lth_jc::JsonContainer& getModuleConfig(const std::string& module);

  private:
    bool initialized_;
    std::map<std::string, Base_ptr> defaults_;
    std::string config_file_;
    std::function<int(std::vector<std::string>)> start_function_;
    std::map<std::string, lth_jc::JsonContainer> module_config_;

    Configuration();
    void parseConfigFile();
    void defineDefaultValues();
    void setDefaultValues();
};

}  // namespace CthunAgent

#endif  // SRC_CONFIGURATION_H_
