#ifndef SRC_CONFIGURATION_H_
#define SRC_CONFIGURATION_H_

#include "errors.h"

#include <horsewhisperer/horsewhisperer.h>
#include <INIReader.h>

namespace CthunAgent {

namespace HW = HorseWhisperer;

static const std::string VERSION_STRING = "cthun-agent version - 0.0.1\n";

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
    static Configuration& Instance() {
        static Configuration instance;
        return instance;
    }

    int initialize(int argc, char *argv[]);

    void setStartFunction(std::function<int(std::vector<std::string>)> start_function);

    template <typename T>
    T get(std::string flagname) {
        if (!configured_) {
            throw unconfigured_error { "cannot get configuration value until initialized" };
        }
        return HW::GetFlag<T>(flagname);
    }

    template<typename T>
    void set(std::string flagname, T value) {
        if (!configured_) {
            throw unconfigured_error { "cannot set configuration value until initialized" };
        }
        HW::SetFlag<T>(flagname, value);
    }

    void validateConfiguration(int parse_result);

  private:
    bool configured_ = false;
    std::vector<std::unique_ptr<EntryBase>> defaults_;
    std::string config_file_ = "";
    std::function<int(std::vector<std::string>)> start_function_;

    Configuration();
    void parseConfigFile();
};

}  // namespace CthunAgent

#endif  // SRC_CONFIGURATION_H_
