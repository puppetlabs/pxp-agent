#include "configuration.h"
#include "file_utils.h"

namespace CthunAgent {

Configuration::Configuration() {
    HW::SetAppName("cthun-agent");
    HW::SetHelpBanner("Usage: cthun-agent [options]");
    HW::SetVersion(VERSION_STRING);

    if (FileUtils::fileExists(FileUtils::shellExpand("~/.cthun-agent"))) {
        config_file_ = FileUtils::shellExpand("~/.cthun-agent");
    // TODO(ploubser): This will have to changed when the AIO agent is done
    } else if (FileUtils::fileExists("/etc/puppetlabs/agent/cthun.cfg")) {
        config_file_ = "/etc/puppetlabs/agent/cthun.cfg";
    }

    // configure the default values
    defaults_.push_back(Base_ptr(
        new Entry<std::string>("server",
                               "s",
                               "cthun servers url",
                               Types::String,
                               "")));

    defaults_.push_back(Base_ptr(
        new Entry<std::string>("ca",
                               "",
                               "CA certificate",
                               Types::String,
                               "")));

    defaults_.push_back(Base_ptr(
        new Entry<std::string>("cert",
                               "",
                               "cthun-agent certificate",
                               Types::String,
                               "")));

    defaults_.push_back(Base_ptr(
        new Entry<std::string>("key",
                               "",
                               "cthun-agent private key",
                               Types::String,
                               "")));

    defaults_.push_back(Base_ptr(
        new Entry<std::string>("logfile",
                               "",
                               "log file (defaults to console logging",
                               Types::String,
                               "")));

    defaults_.push_back(Base_ptr(
        new Entry<std::string>("config-file",
                               "",
                               "specify a non default config file to use",
                               Types::String,
                               "")));
    defaults_.push_back(Base_ptr(
        new Entry<std::string>("spool-dir",
                               "",
                               "specify directory to spool delayed results to",
                               Types::String,
                               DEFAULT_ACTION_RESULTS_DIR)));
}

int Configuration::initialize(int argc, char *argv[]) {
    for (const auto& entry : defaults_) {
        std::string flag_names = entry->name;

        if (!entry->aliases.empty()) {
            flag_names += " " + entry->aliases;
        }

        switch (entry->type) {
            case Integer:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) entry.get();
                    HW::DefineGlobalFlag<int>(flag_names, entry_ptr->help,
                                              entry_ptr->value, [entry_ptr] (int v) -> bool{
                        entry_ptr->configured = true;
                        return true;
                    });
                }
                break;
            case Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) entry.get();
                    HW::DefineGlobalFlag<bool>(flag_names, entry_ptr->help,
                                               entry_ptr->value, [entry_ptr] (bool v) -> bool{
                        entry_ptr->configured = true;
                        return true;
                    });
                }
                break;
            case Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) entry.get();
                    HW::DefineGlobalFlag<double>(flag_names, entry_ptr->help,
                                                 entry_ptr->value, [entry_ptr] (double v) -> bool{
                        entry_ptr->configured = true;
                        return true;
                    });
                }
                break;
            default:
                {
                    Entry<std::string>* entry_ptr = (Entry<std::string>*) entry.get();
                    HW::DefineGlobalFlag<std::string>(flag_names, entry_ptr->help,
                                                      entry_ptr->value, [entry_ptr] (std::string v) -> bool {
                        entry_ptr->configured = true;
                        return true;
                    });
                }
        }
    }

    HW::DefineAction("start", 0, false, "Start the agent (Default)", "Start the agent", start_function_);
    HW::DefineActionFlag<std::string>("start", "module-dir", "External module directory",
                                      "", nullptr);

    // manipulate argc and v to make start the default action.
    // TODO(ploubser): Add ability to specify default action to HorseWhisperer
    int modified_argc = argc + 1;
    char* modified_argv[modified_argc];
    char action[] = "start";

    for (int i = 0; i < argc; i++) {
        modified_argv[i] = argv[i];
    }
    modified_argv[modified_argc - 1] = action;

    int parse_result { HW::Parse(modified_argc, modified_argv) };

    if (!HW::GetFlag<std::string>("config-file").empty()) {
        config_file_ = HW::GetFlag<std::string>("config-file");
    }

    if (!config_file_.empty()) {
        parseConfigFile();
    }

    validateConfiguration(parse_result);
    configured_ = true;

    return parse_result;
}

void Configuration::setStartFunction(std::function<int(std::vector<std::string>)> start_function) {
    start_function_ = start_function;
}


void Configuration::validateConfiguration(int parse_result) {
    if (parse_result == HW::PARSE_ERROR || parse_result == HW::PARSE_INVALID_FLAG) {
        throw cli_parse_error { "An error occurred while parsing cli options"};
    }

    // determine which of your values must be initalised
    if (HW::GetFlag<std::string>("server").empty()) {
        throw required_not_set_error { "server value must be defined" };
    } else if (HW::GetFlag<std::string>("server").find("wss://") != 0) {
        throw required_not_set_error { "server value must start with wss://" };
    }

    if (HW::GetFlag<std::string>("ca").empty()) {
        throw required_not_set_error { "ca value must be defined" };
    } else if (!FileUtils::fileExists(HW::GetFlag<std::string>("ca"))) {
        throw required_not_set_error { "ca file not found" };
    }

    if (HW::GetFlag<std::string>("cert").empty()) {
        throw required_not_set_error { "cert value must be defined" };
    } else if (!FileUtils::fileExists(HW::GetFlag<std::string>("cert"))) {
        throw required_not_set_error { "cert file not found" };
    }

    if (HW::GetFlag<std::string>("key").empty()) {
        throw required_not_set_error { "key value must be defined" };
    } else if (!FileUtils::fileExists(HW::GetFlag<std::string>("key"))) {
        throw required_not_set_error { "key file not found" };
    }

    if (!HW::GetFlag<std::string>("spool-dir").empty()) {
        std::string spool_dir = FileUtils::shellExpand(HW::GetFlag<std::string>("spool-dir"));
        if (spool_dir[-1] != '/') {
            HW::SetFlag<std::string>("spool-dir", spool_dir + "/");
        }
    }
}

void Configuration::parseConfigFile() {
    INIReader reader { config_file_ };

    for (const auto& entry : defaults_) {
        // skip config entry if flag was set
        if (entry->configured) {
            continue;
        }
        switch (entry->type) {
            case Integer:
                {
                    Entry<int>* entry_ptr = (Entry<int>*) entry.get();
                    HW::SetFlag<int>(entry_ptr->name,
                                     reader.GetInteger("",
                                                       entry_ptr->name,
                                                       entry_ptr->value));
                }
                break;
            case Bool:
                {
                    Entry<bool>* entry_ptr = (Entry<bool>*) entry.get();
                    HW::SetFlag<bool>(entry_ptr->name,
                                      reader.GetBoolean("",
                                                        entry_ptr->name,
                                                        entry_ptr->value));
                }
                break;
            case Double:
                {
                    Entry<double>* entry_ptr = (Entry<double>*) entry.get();
                    HW::SetFlag<double>(entry_ptr->name,
                                        reader.GetReal("",
                                                       entry_ptr->name,
                                                       entry_ptr->value));
                }
                break;
            default:
                {
                    Entry<std::string>* entry_ptr = (Entry<std::string>*) entry.get();
                    HW::SetFlag<std::string>(entry_ptr->name,
                                             reader.Get("",
                                                        entry_ptr->name,
                                                        entry_ptr->value));
                }
        }
    }
}

}  // namespace CthunAgent
