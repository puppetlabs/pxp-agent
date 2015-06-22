#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/uuid.hpp>
#include <cthun-agent/string_utils.hpp>
#include <cthun-agent/rpc_schemas.hpp>
#include <cthun-agent/external_module.hpp>
#include <cthun-agent/modules/echo.hpp>
#include <cthun-agent/modules/inventory.hpp>
#include <cthun-agent/modules/ping.hpp>
#include <cthun-agent/modules/status.hpp>

#include <cthun-client/connector/errors.hpp>
#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/validator/validator.hpp>  // Validator
#include <cthun-client/protocol/schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>
#include <memory>
#include <stdexcept>  // out_of_range

namespace CthunAgent {

namespace fs = boost::filesystem;

static const std::string AGENT_CLIENT_TYPE { "agent" };

Agent::Agent(const std::string& modules_dir,
             const std::string& server_url,
             const std::string& ca,
             const std::string& crt,
             const std::string& key)
        try
            : connector_ptr_ { new CthunClient::Connector(server_url,
                                                          AGENT_CLIENT_TYPE,
                                                          ca,
                                                          crt,
                                                          key) },
              modules_ {},
              request_processor_ { connector_ptr_ } {
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
    // TODO(ale): add associate response callback

    connector_ptr_->registerMessageCallback(
        RPCSchemas::BlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            blockingRequestCallback(parsed_chunks);
        });

    connector_ptr_->registerMessageCallback(
        RPCSchemas::NonBlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            nonBlockingRequestCallback(parsed_chunks);
        });

    try {
        connector_ptr_->connect();
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
        connector_ptr_->monitorConnection();
    } catch (CthunClient::connection_fatal_error) {
        throw fatal_error { "failed to reconnect" };
    }
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
        LOG_WARNING("Failed to locate the modules directory; no external "
                    "module will be loaded");
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
            actions_list += action;
        }

        auto txt_suffix = StringUtils::plural(module.second->actions.size());
        LOG_INFO("Loaded '%1%' module - %2%%3%%4%",
                 module.first, txt, txt_suffix, actions_list);
    }
}

void Agent::blockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    validateAndProcessRequest(parsed_chunks, RequestType::Blocking);
}

void Agent::nonBlockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    validateAndProcessRequest(parsed_chunks, RequestType::NonBlocking);
}

void Agent::validateAndProcessRequest(
                const CthunClient::ParsedChunks& parsed_chunks,
                const RequestType& request_type) {
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    auto requester = parsed_chunks.envelope.get<std::string>("sender");
    LOG_INFO("Received %1% request %2% by %3%",
             requestTypeNames[request_type], request_id, requester);
    LOG_DEBUG("Request %1%:\n%2%", request_id, parsed_chunks.toString());

    try {
        validateRequestFormat(parsed_chunks);
    } catch (request_validation_error& e) {
        // Bad message; send a *Cthun core error*
        LOG_ERROR("Invalid %1% request by %2%: %3%",
                  request_id, requester, e.what());
        CthunClient::DataContainer cthun_error_data {};
        cthun_error_data.set<std::string>("id", request_id);
        cthun_error_data.set<std::string>("description", e.what());

        try {
            connector_ptr_->send(std::vector<std::string> { requester },
                                 CthunClient::Protocol::ERROR_MSG_TYPE,
                                 DEFAULT_MSG_TIMEOUT_SEC,
                                 cthun_error_data);
            LOG_INFO("Replied to %1% request %2% by %3% with a Cthun core "
                     "error message", requestTypeNames[request_type],
                     request_id, requester);
        } catch (CthunClient::connection_error& e) {
            LOG_ERROR("Failed to send Cthun core error message for request %1% "
                      "by %2%: %3%", request_id, requester, e.what());
        }
    }

    auto transaction_id = parsed_chunks.data.get<std::string>("transaction_id");
    LOG_INFO("About to process %1% request %2% by %3%, transaction %4%",
             requestTypeNames[request_type], request_id, requester,
             transaction_id);

    try {
        validateRequestContent(parsed_chunks);
        processRequest(parsed_chunks, request_type);
    } catch (request_error& e) {
        // Invalid request or process failure; send an *RPC error*
        LOG_ERROR("Failed to process request %1% by %2%, transaction %3%: %4%",
                  request_id, requester, transaction_id, e.what());
        CthunClient::DataContainer rpc_error_data {};
        rpc_error_data.set<std::string>("transaction_id", transaction_id);
        rpc_error_data.set<std::string>("id", request_id);
        rpc_error_data.set<std::string>("description", e.what());

        try {
            connector_ptr_->send(std::vector<std::string> { requester },
                                 RPCSchemas::RPC_ERROR_MSG_TYPE,
                                 DEFAULT_MSG_TIMEOUT_SEC,
                                 rpc_error_data);
            LOG_INFO("Replied to %1% request %2% by %3%, transaction %4%, with "
                     "an RPC error message", requestTypeNames[request_type],
                     request_id, requester, transaction_id);
        } catch (CthunClient::connection_error& e) {
            LOG_ERROR("Failed to send RPC error message for request %1% by "
                      "%2%, transaction %3% (no further attempts): %4%",
                      request_id, requester, transaction_id, e.what());
        }
    }
}

void Agent::validateRequestFormat(const CthunClient::ParsedChunks& parsed_chunks) {
    if (!parsed_chunks.has_data) {
        throw request_validation_error { "no data" };
    }
    if (parsed_chunks.invalid_data) {
        throw request_validation_error { "invalid data" };
    }
    // NOTE(ale): currently, we don't support ContentType::Binary
    if (parsed_chunks.data_type != CthunClient::ContentType::Json) {
        throw request_validation_error { "data is not in JSON format" };
    }
}

void Agent::validateRequestContent(const CthunClient::ParsedChunks& parsed_chunks) {
    auto module_name = parsed_chunks.data.get<std::string>("module");
    auto action_name = parsed_chunks.data.get<std::string>("action");

    try {
        if (!modules_.at(module_name)->hasAction(action_name)) {
            throw request_validation_error { "unknown action '" + action_name
                                             + "' for module " + module_name };
        }
    } catch (std::out_of_range& e) {
        throw request_validation_error { "unknown module: " + module_name };
    }
}

void Agent::processRequest(const CthunClient::ParsedChunks& parsed_chunks,
                           const RequestType& request_type) {
    auto module_name = parsed_chunks.data.get<std::string>("module");
    auto action_name = parsed_chunks.data.get<std::string>("action");
    auto module_ptr = modules_.at(module_name);

    if (request_type == RequestType::Blocking) {
        request_processor_.processBlockingRequest(module_ptr,
                                                  action_name,
                                                  parsed_chunks);
    } else {
        request_processor_.processNonBlockingRequest(module_ptr,
                                                     action_name,
                                                     parsed_chunks);
    }
}

}  // namespace CthunAgent
