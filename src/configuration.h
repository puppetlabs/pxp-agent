#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include "errors.h"

#include <horsewhisperer/horsewhisperer.h>
#include <INIReader.h>

#include <map>

namespace CthunAgent {

namespace HW = HorseWhisperer;

static const std::string VERSION_STRING = "cthun-agent version - 0.0.1\n";
static const std::string DEFAULT_ACTION_RESULTS_DIR = "/tmp/cthun-agent/";

enum Types { Integer, String, Bool, Double};

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
    // TODO(ale): implement reset method (change singleton structure)
    static Configuration& Instance() {
        static Configuration instance;
        return instance;
    }

    int initialize(int argc, char *argv[]);

    void setStartFunction(std::function<int(std::vector<std::string>)> start_function);

    // TODO(ale): HW::GetFlag should use boost::optional or make
    // the default value explicit instead of using the default ctor

    /// If Configuration was already initialized, return the value
    /// set for the specified flag (NB: the value can be a default).
    /// If Configuration was not initialized so far, but the requested
    /// flag is known, return the default value; throw a
    /// configuration_entry_error otherwise.
    template <typename T>
    T get(std::string flagname) {
        if (initialized_) {
            return HW::GetFlag<T>(flagname);
        }

        // Configuration instance not initialized; try to get default
        auto entry = defaults_.find(flagname);
        if (entry != defaults_.end()) {
            Entry<T>* entry_ptr = (Entry<T>*) entry->second.get();
            return entry_ptr->value;
        }

        throw configuration_entry_error { "no default value for " + flagname };
    }

    // TODO(ale): HW::SetFlag should specify the outcome (unknown flag
    // or invalid value) as done by HW::Parse

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

        auto failure = HW::SetFlag<T>(flagname, value);
        if (failure) {
            throw configuration_entry_error { "unknown flag name or invalid "
                                              "value for " + flagname };
        }
    }

    void validateConfiguration(int parse_result);

  private:
    bool initialized_ = false;
    std::map<std::string, Base_ptr> defaults_;
    std::string config_file_ = "";
    std::function<int(std::vector<std::string>)> start_function_;

    Configuration();
    void parseConfigFile();
};

}  // namespace CthunAgent

#endif  // SRC_CONFIGURATION_H_
