#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include <cthun-agent/errors.hpp>

#include <horsewhisperer/horsewhisperer.h>
#include <INIReader.h>

#include <map>

namespace CthunAgent {

namespace HW = HorseWhisperer;


static const std::string VERSION_STRING = "cthun-agent version - 0.0.1\n";
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
    /// Return a numeric code indicating the parsing outcome (refer to
    /// HorseWhisperer).
    /// Throw a cli_parse_error in case it fails to parse the CLI
    /// arguments.
    /// Throw a required_not_set_error in case of a missing or invalid
    /// configuration value.
    /// Throw a configuration_entry_error if the specified config file
    /// does not exist.
    int initialize(int argc, char *argv[]);

    /// Set the start function that will be executed when calling
    /// HorseWhisperer::Start().
    void setStartFunction(std::function<int(std::vector<std::string>)> start_function);

    /// If Configuration was already initialized, return the value
    /// set for the specified flag (NB: the value can be a default);
    /// throw a configuration_entry_error such flag is unknown.
    /// If Configuration was not initialized so far, but the requested
    /// flag is known, return the default value; throw a
    /// configuration_entry_error otherwise.
    template <typename T>
    T get(std::string flagname) {
        if (initialized_) {
            try {
                return HW::GetFlag<T>(flagname);
            } catch (HW::undefined_flag_error& e) {
                throw configuration_entry_error { std::string { e.what() } };
            }
        }

        // Configuration instance not initialized; get default
        auto entry = defaults_.find(flagname);
        if (entry == defaults_.end()) {
            throw configuration_entry_error { "no default value for " + flagname };
        } else {
            Entry<T>* entry_ptr = (Entry<T>*) entry->second.get();
            return entry_ptr->value;
        }
    }

    /// Set the specified value for a given configuration flag.
    /// Throw an uninitialized_error in case the Configuration was not
    /// initialized so far.
    /// Throw a configuration_entry_error in case the specified flag
    /// is unknown or the value is invalid.
    template<typename T>
    void set(std::string flagname, T value) {
        if (!initialized_) {
            throw uninitialized_error { "cannot set configuration value until "
                                        "initialized" };
        }

        try {
            HW::SetFlag<T>(flagname, value);
        } catch (HW::flag_validation_error) {
            throw configuration_entry_error { "invalid value for " + flagname };
        } catch (HW::undefined_flag_error) {
            throw configuration_entry_error { "unknown flag name: " + flagname };
        }
    }

    /// Ensure all required values are valid. If necessary, expand
    /// file paths to the expected format.
    /// Throw a required_not_set_error if any required file path is
    /// missing; a configuration_entry_error in case of invalid values.
    void validateAndNormalizeConfiguration();

  private:
    bool initialized_;
    std::map<std::string, Base_ptr> defaults_;
    std::string config_file_;
    std::function<int(std::vector<std::string>)> start_function_;

    Configuration();
    void parseConfigFile();
    void defineDefaultValues();
    void defineRelativeValues(std::string bin_path);
    void setDefaultValues();
};

}  // namespace CthunAgent

#endif  // SRC_CONFIGURATION_H_
