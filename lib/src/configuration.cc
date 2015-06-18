#include <cthun-agent/configuration.hpp>

#include <leatherman/file_util/file.hpp>

#include "version-inl.hpp"

#include <boost/filesystem/operations.hpp>

namespace CthunAgent {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;

const std::string DEFAULT_MODULES_DIR { "/usr/share/cthun-agent/modules" };

//
// Private
//

Configuration::Configuration() : initialized_ { false },
                                 defaults_ {},
                                 config_file_ { "" },
                                 start_function_ {} {
    defineDefaultValues();
}

void Configuration::defineDefaultValues() {
    HW::SetAppName("cthun-agent");
    HW::SetHelpBanner("Usage: cthun-agent [options]");
    HW::SetVersion(CTHUN_AGENT_VERSION);

    // start setting the config file path to known existent locations;
    // HW will overwrite it with the one parsed from CLI, if specified
    if (lth_file::file_readable(lth_file::tilde_expand("~/.cthun-agent"))) {
        config_file_ = lth_file::tilde_expand("~/.cthun-agent");
    // TODO(ploubser): This will have to changed when the AIO agent is done
    } else if (lth_file::file_readable("/etc/puppetlabs/agent/cthun.cfg")) {
        config_file_ = "/etc/puppetlabs/agent/cthun.cfg";
    }

    std::string modules_dir { "" };

    if (fs::is_directory(DEFAULT_MODULES_DIR)) {
        modules_dir = DEFAULT_MODULES_DIR;
    }

    defaults_.insert(std::pair<std::string, Base_ptr>("server", Base_ptr(
        new Entry<std::string>("server",
                               "s",
                               "Cthun server URL",
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
                               "cthun-agent certificate",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("key", Base_ptr(
        new Entry<std::string>("key",
                               "",
                               "cthun-agent private key",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("logfile", Base_ptr(
        new Entry<std::string>("logfile",
                               "",
                               "Log file (defaults to console logging)",
                               Types::String,
                               ""))));

    defaults_.insert(std::pair<std::string, Base_ptr>("config-file", Base_ptr(
        new Entry<std::string>("config-file",
                               "",
                               "Specify a non default config file to use",
                               Types::String,
                               config_file_))));

    defaults_.insert(std::pair<std::string, Base_ptr>("spool-dir", Base_ptr(
        new Entry<std::string>("spool-dir",
                               "",
                               "Specify directory to spool delayed results to",
                               Types::String,
                               DEFAULT_ACTION_RESULTS_DIR))));

    defaults_.insert(std::pair<std::string, Base_ptr>("modules-dir", Base_ptr(
        new Entry<std::string>("modules-dir",
                               "",
                               "Specify directory containing external modules",
                               Types::String,
                               modules_dir))));
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
    if (!lth_file::file_readable(config_file_)) {
        throw configuration_entry_error { config_file_ + " does not exist" };
    }

    INIReader reader { config_file_ };

    for (const auto& entry : defaults_) {
        // skip config entry if flag was set
        if (entry.second->configured) {
            continue;
        }
        switch (entry.second->type) {
            case Integer:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) entry.second.get();
                    HW::SetFlag<int>(entry_ptr->name,
                                     reader.GetInteger("",
                                                       entry_ptr->name,
                                                       entry_ptr->value));
                }
                break;
            case Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) entry.second.get();
                    HW::SetFlag<bool>(entry_ptr->name,
                                      reader.GetBoolean("",
                                                        entry_ptr->name,
                                                        entry_ptr->value));
                }
                break;
            case Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) entry.second.get();
                    HW::SetFlag<double>(entry_ptr->name,
                                        reader.GetReal("",
                                                       entry_ptr->name,
                                                       entry_ptr->value));
                }
                break;
            default:
                {
                    Entry<std::string>* entry_ptr =
                        (Entry<std::string>*) entry.second.get();
                    HW::SetFlag<std::string>(entry_ptr->name,
                                             reader.Get("",
                                                        entry_ptr->name,
                                                        entry_ptr->value));
                }
        }
    }
}

//
// Public interface
//

void Configuration::reset() {
    HW::Reset();
    setDefaultValues();
    initialized_ = false;
}

HW::ParseResult Configuration::initialize(int argc, char *argv[]) {
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

    if (parse_result == HW::ParseResult::ERROR
        || parse_result == HW::ParseResult::INVALID_FLAG) {
        throw cli_parse_error { "An error occurred while parsing cli options"};
    }

    config_file_ = HW::GetFlag<std::string>("config-file");
    if (!config_file_.empty()) {
        parseConfigFile();
    }

    validateAndNormalizeConfiguration();
    initialized_ = true;

    return parse_result;
}

void Configuration::setStartFunction(std::function<int(std::vector<std::string>)> start_function) {
    start_function_ = start_function;
}

void Configuration::validateAndNormalizeConfiguration() {
    // determine which of your values must be initalised
    if (HW::GetFlag<std::string>("server").empty()) {
        throw required_not_set_error { "server value must be defined" };
    } else if (HW::GetFlag<std::string>("server").find("wss://") != 0) {
        throw configuration_entry_error { "server value must start with wss://" };
    }

    if (HW::GetFlag<std::string>("ca").empty()) {
        throw required_not_set_error { "ca value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("ca"))) {
        throw configuration_entry_error { "ca file not found" };
    }

    if (HW::GetFlag<std::string>("cert").empty()) {
        throw required_not_set_error { "cert value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("cert"))) {
        throw configuration_entry_error { "cert file not found" };
    }

    if (HW::GetFlag<std::string>("key").empty()) {
        throw required_not_set_error { "key value must be defined" };
    } else if (!lth_file::file_readable(HW::GetFlag<std::string>("key"))) {
        throw configuration_entry_error { "key file not found" };
    }

    for (const auto& flag_name : std::vector<std::string> { "ca", "cert", "key" }) {
        const auto& path = HW::GetFlag<std::string>(flag_name);
        HW::SetFlag<std::string>(flag_name, lth_file::tilde_expand(path));
    }

    if (HW::GetFlag<std::string>("spool-dir").empty())  {
        // Unexpected, since we have a default value for spool-dir
        throw required_not_set_error { "spool-dir must be defined" };
    } else {
        // TODO(ale): ensure that spool_dir_ is a directory, once we
        // have leatherman::file_util

        std::string spool_dir = lth_file::tilde_expand(HW::GetFlag<std::string>("spool-dir"));
        if (spool_dir.back() != '/') {
            HW::SetFlag<std::string>("spool-dir", spool_dir + "/");
        }
    }
}

}  // namespace CthunAgent
