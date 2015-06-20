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
            : connector_ptr_ { new CthunConnector(server_url, AGENT_CLIENT_TYPE,
                                                  ca, crt, key) },
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
    validateAndProcessRequest(RequestType::Blocking, parsed_chunks);
}

void Agent::nonBlockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    validateAndProcessRequest(RequestType::NonBlocking, parsed_chunks);
}

void Agent::validateAndProcessRequest(
                const RequestType& request_type,
                const CthunClient::ParsedChunks& parsed_chunks) {
    try {
        // Inspect and validate the request message format
        ActionRequest request { request_type, parsed_chunks };

        LOG_INFO("About to process %1% request %2% by %3%, transaction %4%",
                 requestTypeNames[request_type], request.id(), request.sender(),
                 request.transactionId());

        try {
            // We can access the request content; validate it
            validateRequestContent(request);
        } catch (request_validation_error& e) {
            // Invalid request; send an *RPC error*

            LOG_ERROR("Invalid %1% request %2% by %3%, transaction %4%: %5%",
                      requestTypeNames[request_type], request.id(),
                      request.sender(), request.transactionId(), e.what());
            connector_ptr_->sendRPCError(request, e.what());
            return;
        }

        // RequestProcessor deals with possible proccessing errors
        request_processor_.processRequest(modules_[request.module()], request);
    } catch (request_format_error& e) {
        // Bad message; send a *Cthun core error*

        auto id = parsed_chunks.envelope.get<std::string>("id");
        auto sender = parsed_chunks.envelope.get<std::string>("sender");
        std::vector<std::string> endpoints { sender };
        LOG_ERROR("Invalid %1% request by %2%: %3%", id, sender, e.what());
        connector_ptr_->sendCthunError(id, e.what(), endpoints);
    }
}

void Agent::validateRequestContent(const ActionRequest& request) {
    try {
        if (!modules_.at(request.module())->hasAction(request.action())) {
            throw request_validation_error { "unknown action '" + request.action()
                                             + "' for module " + request.module() };
        }
    } catch (std::out_of_range& e) {
        throw request_validation_error { "unknown module: " + request.module() };
    }
}

}  // namespace CthunAgent
