#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/uuid.hpp>
#include <cthun-agent/string_utils.hpp>
#include <cthun-agent/external_module.hpp>
#include <cthun-agent/modules/echo.hpp>
#include <cthun-agent/modules/inventory.hpp>
#include <cthun-agent/modules/ping.hpp>
#include <cthun-agent/modules/status.hpp>

#include <cthun-client/connector/errors.hpp>
#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/validator/validator.hpp>  // Validator

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>
#include <memory>

namespace CthunAgent {

namespace fs = boost::filesystem;

//
// Constants
//

static const std::string AGENT_CLIENT_TYPE { "agent" };
static const std::string CTHUN_REQUEST_SCHEMA_NAME { "http://puppetlabs.com/cnc_request" };
static const std::string CTHUN_RESPONSE_SCHEMA_NAME { "http://puppetlabs.com/cnc_response" };
static const std::string CTHUN_ERROR_SCHEMA_NAME { "http://puppetlabs.com/cnc_error_message" };

static const int DEFAULT_MSG_TIMEOUT_SEC { 10 };

//
// Agent
//

Agent::Agent(const std::string& modules_dir,
             const std::string& server_url,
             const std::string& ca,
             const std::string& crt,
             const std::string& key)
        try
            : connector_ { server_url, AGENT_CLIENT_TYPE, ca, crt, key },
              modules_ {} {
    // NB: certificate paths are validated by HW
    loadInternalModules();

    if (!modules_dir.empty()) {
        loadExternalModulesFrom(modules_dir);
    } else {
        LOG_INFO("The modules directory was not provided; no external module "
                 "will be loaded");
    }

    logLoadedModules();
} catch (CthunClient::connection_config_error& e) {
    throw fatal_error { std::string("failed to configure the agent: ")
                        + e.what() };
}

void Agent::start() {
    connector_.registerMessageCallback(
        getCncRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            cncRequestCallback(parsed_chunks);
        });

    try {
        connector_.connect();
    } catch (CthunClient::connection_config_error& e) {
        LOG_ERROR("Failed to configure the underlying communications layer: %1%",
                  e.what());
        throw fatal_error { "failed to configure the underlying communications"
                            "layer" };
    } catch (CthunClient::connection_fatal_error& e) {
        LOG_ERROR("Failed to connect: %1%", e.what());
        throw fatal_error { "failed to connect" };
    }

    // The agent is now connected and the request handlers are set;
    // we can now call the monitoring method that will block this
    // thread of execution.
    // Note that, in case the underlying connection drops, the
    // connector will keep trying to re-establish it indefinitely
    // (the max_connect_attempts is 0 by default).
    try {
        connector_.monitorConnection();
    } catch (CthunClient::connection_fatal_error) {
        throw fatal_error { "failed to reconnect" };
    }

    // TODO(ale): wait for the associate session response, once the
    // associated state is exposed by Connector
}

//
// Agent - private interface
//

void Agent::loadInternalModules() {
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);
}

void Agent::loadExternalModulesFrom(fs::path dir_path) {
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

void Agent::logLoadedModules() const {
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

CthunClient::Schema Agent::getCncRequestSchema() const {
    CthunClient::Schema schema { CTHUN_REQUEST_SCHEMA_NAME,
                                 CthunClient::ContentType::Json };
    using T_C = CthunClient::TypeConstraint;
    schema.addConstraint("module", T_C::String, true);
    schema.addConstraint("action", T_C::String, true);
    // TODO(ale): evaluate changing params; ambiguous
    schema.addConstraint("params", T_C::Object, false);
    return schema;
}

void Agent::cncRequestCallback(const CthunClient::ParsedChunks& parsed_chunks) {
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    auto requester_endpoint = parsed_chunks.envelope.get<std::string>("sender");

    LOG_INFO("Received message %1% from %2%", request_id, requester_endpoint);
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

        if (modules_.find(module_name) == modules_.end()) {
            throw request_validation_error { "unknown module: " + module_name };
        }

        // Perform the requested action
        auto module_ptr = modules_.at(module_name);
        auto data_json = module_ptr->performRequest(action_name, parsed_chunks);

        // Wrap debug data
        std::vector<CthunClient::DataContainer> debug {};
        for (auto& debug_entry : parsed_chunks.debug) {
            debug.push_back(debug_entry);
        }

        try {
            connector_.send(std::vector<std::string> { requester_endpoint },
                            CTHUN_RESPONSE_SCHEMA_NAME,
                            DEFAULT_MSG_TIMEOUT_SEC,
                            data_json,
                            debug);
        } catch (CthunClient::connection_error& e) {
            // We failed to send the response; it's up to the
            // requester to ask again
            LOG_ERROR("Failed to reply to the %1% request from %2%: %3%",
                      request_id, requester_endpoint, e.what());
        }
    } catch (request_error& e) {
        LOG_ERROR("Failed to process message %1% from %2%: %3%",
                  request_id, requester_endpoint, e.what());
        CthunClient::DataContainer error_data_json;
        error_data_json.set<std::string>("description", e.what());
        error_data_json.set<std::string>("id", request_id);

        try {
            connector_.send(std::vector<std::string> { requester_endpoint },
                            CTHUN_ERROR_SCHEMA_NAME,
                            DEFAULT_MSG_TIMEOUT_SEC,
                            error_data_json);
            LOG_INFO("Replied to message %1% from %2% with an error message",
                     request_id, requester_endpoint);
        } catch (CthunClient::connection_error& e) {
            LOG_ERROR("Failed send an error response to the %1% request "
                      "from %2%: %3%", request_id, requester_endpoint, e.what());
        }
    }
}

}  // namespace CthunAgent
