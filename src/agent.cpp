#include "src/agent.h"
#include "src/errors.h"
#include "src/log.h"
#include "src/uuid.h"
#include "src/string_utils.h"

#include "src/external_module.h"
#include "src/modules/echo.h"
#include "src/modules/inventory.h"
#include "src/modules/ping.h"
#include "src/modules/status.h"

#include <cthun-client/src/connector/errors.h>
#include <cthun-client/src/data_container/data_container.h>
#include <cthun-client/src/validator/validator.h>  // Validator

#include <vector>
#include <memory>

LOG_DECLARE_NAMESPACE("agent");

namespace CthunAgent {

namespace fs = boost::filesystem;

//
// Constants
//

static const std::string AGENT_CLIENT_TYPE { "agent" };
static const std::string SCHEMA_NAME_CNC { "cnc_schema" };

static const int DEFAULT_MSG_TIMEOUT_SEC { 10 };

//
// Free functions
//

fs::path getModulesDirPath(const std::string& bin_path) {
    // TODO(ale): CTH-76 - this doesn't work if bin_path (argv[0]) has
    // only the name of the executable, neither when cthun_agent is
    // called by a symlink. The only safe way to refer to external
    // modules is to store them in a known location (ex. ~/cthun/).

    fs::path modules_dir_path {
        fs::canonical(
            fs::system_complete(
                fs::path(bin_path)).parent_path().parent_path()) };
    modules_dir_path += "/modules";
    return modules_dir_path;
}

//
// Agent
//

Agent::Agent(const std::string& bin_path,
             const std::string& server_url,
             const std::string& ca,
             const std::string& crt,
             const std::string& key)
        try : connector_ { server_url, AGENT_CLIENT_TYPE, ca, crt, key },
              modules_ {} {
    // NB: certificate paths are validated by HW
    loadInternalModules_();
    auto modules_dir_path = getModulesDirPath(bin_path);
    loadExternalModulesFrom_(modules_dir_path);
    logLoadedModules_();
} catch (CthunClient::connection_config_error& e) {
    throw fatal_error { std::string("failed to configure the agent: ")
                        + e.what() };
}

void Agent::start() {
    connector_.registerMessageCallback(
        getCncRequestSchema_(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            cncRequestCallback_(parsed_chunks);
        });

    try {
        connector_.connect();
        connector_.enablePersistence();
    } catch (CthunClient::connection_config_error& e) {
        LOG_ERROR("Failed to configure the underlying communications layer: %1%",
                  e.what());
        throw fatal_error { "failed to configure the underlying communications"
                            "layer" };
    } catch (CthunClient::connection_fatal_error& e) {
        LOG_ERROR("Failed to connect: %1%", e.what());
        throw fatal_error { "failed to connect" };
    }

    while(true) {
        sleep(10);
    }
}

//
// Agent - private interface
//

void Agent::loadInternalModules_() {
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);
}

void Agent::loadExternalModulesFrom_(fs::path dir_path) {
    LOG_INFO("Loading external modules from %1%", dir_path.string());

    if (fs::is_directory(dir_path)) {
        fs::directory_iterator end;

        for (auto f = fs::directory_iterator(dir_path); f != end; ++f) {
            if (!fs::is_directory(f->status())) {
                auto f_p = f->path().string();

                try {
                    ExternalModule* e_m = new ExternalModule(f_p);
                    modules_[e_m->module_name] = std::shared_ptr<Module>(e_m);
                } catch (module_error& e) {
                    LOG_ERROR("Failed to load %1%; %2%", f_p, e.what());
                } catch (std::exception& e) {
                    LOG_ERROR("Unexpected error when loading %1%; %2%",
                              f_p, e.what());
                } catch (...) {
                    LOG_ERROR("Unexpected error when loading %1%", f_p);
                }
            }
        }
    } else {
        LOG_WARNING("Failed to locate the modules directory; external modules "
                    "will not be loaded");
    }
}

void Agent::logLoadedModules_() const {
    for (auto& module : modules_) {
        std::string txt { "found no action" };
        std::string actions_list { "" };

        for (auto& action : module.second->actions) {
            if (actions_list.empty()) {
                txt = "action";
                actions_list += ": ";
            } else {
                actions_list += ", ";
            }
            actions_list += action.first;
        }

        auto txt_suffix = StringUtils::plural(module.second->actions.size());
        LOG_INFO("Loaded '%1%' module - %2%%3%%4%",
                 module.first, txt, txt_suffix, actions_list);
    }
}

CthunClient::Schema Agent::getCncRequestSchema_() const {
    CthunClient::Schema schema { CthunClient::CTHUN_REQUEST_SCHEMA_NAME,
                                 CthunClient::ContentType::Json };
    using T_C = CthunClient::TypeConstraint;
    schema.addConstraint("module", T_C::String, true);
    schema.addConstraint("action", T_C::String, true);
    // TODO(ale): evaluate changing params; ambiguous
    schema.addConstraint("params", T_C::String, false);
    return schema;
}

void Agent::cncRequestCallback_(const CthunClient::ParsedChunks& parsed_chunks) {
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    auto sender_endpoint = parsed_chunks.envelope.get<std::string>("sender");

    LOG_INFO("Received message %1% from %2%", request_id, sender_endpoint);
    LOG_DEBUG("Message %1%:\n%2%", request_id, parsed_chunks.toString());

    try {
        // Inspect envelope
        if (!parsed_chunks.has_data) {
            throw request_validation_error { "no data" };
        }

        if (parsed_chunks.data_type != CthunClient::ContentType::Json) {
            throw request_validation_error { "data is not in JSON format" };
        }

        // Inspect data
        auto module_name = parsed_chunks.data.get<std::string>("module");
        auto action_name = parsed_chunks.data.get<std::string>("action");

        if (modules_.find(module_name) != modules_.end()) {
            throw request_validation_error { "unknown module: " + module_name };
        }

        // Perform the requested action
        auto module_ptr = modules_.at(module_name);
        auto data_json = module_ptr->performRequest(action_name, parsed_chunks);

        // Wrap debug data
        std::vector<CthunClient::DataContainer> debug {};
        for (auto& debug_txt : parsed_chunks.debug) {
            CthunClient::DataContainer debug_entry {};
            // TODO(ale): make this consistent with the debug format
            debug_entry.set<std::string>("debug_data", debug_txt);
            debug.push_back(debug_entry);
        }

        try {
            connector_.send(std::vector<std::string> { sender_endpoint },
                            CthunClient::CTHUN_RESPONSE_SCHEMA_NAME,
                            DEFAULT_MSG_TIMEOUT_SEC,
                            data_json,
                            debug);
        } catch (CthunClient::connection_error& e) {
            // We failed to send the response; it's up to the
            // requester to ask again
            LOG_ERROR("Failed to reply to the %1% request from %2%: %3%",
                      request_id, sender_endpoint, e.what());
        }
    } catch (request_error& e) {
        LOG_ERROR("Failed to process message %1% from %2%: %3%",
                  request_id, sender_endpoint, e.what());
        CthunClient::DataContainer error_data_json;

        // TODO(ale): make error handling consistent in our protocol
        // specs; we should have an error schema

        // TODO(ale): we should specify the id of the request; more in
        // general, we should specify the request id in every response

        error_data_json.set<std::string>("error", e.what());

        try {
            connector_.send(std::vector<std::string> { sender_endpoint },
                            CthunClient::CTHUN_RESPONSE_SCHEMA_NAME,
                            DEFAULT_MSG_TIMEOUT_SEC,
                            error_data_json);
        } catch (CthunClient::connection_error& e) {
            LOG_ERROR("Failed send an error response to the %1% request "
                      "from %2%: %3%", request_id, sender_endpoint, e.what());
        }
    }
}

}  // namespace CthunAgent
