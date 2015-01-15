#include "src/agent/agent_endpoint.h"
#include "src/agent/modules/echo.h"
#include "src/agent/modules/inventory.h"
#include "src/agent/modules/ping.h"
#include "src/agent/modules/status.h"
#include "src/agent/external_module.h"
#include "src/agent/schemas.h"
#include "src/agent/errors.h"
#include "src/common/log.h"
#include "src/common/uuid.h"
#include "src/common/string_utils.h"
#include "src/websocket/errors.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

LOG_DECLARE_NAMESPACE("agent.agent_endpoint");

namespace Cthun {
namespace Agent {

static const uint CONNECTION_STATE_CHECK_INTERVAL { 15 };
static const int DEFAULT_MESSAGE_TIMEOUT_IN_SECONDS { 10 };

AgentEndpoint::AgentEndpoint(std::string bin_path) {
    // declare internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    // modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);

    // TODO(ale): CTH-76 this doesn't work if bin_path (= argv[0]) has
    // only the name of the executable, neither when cthun_agent is
    // called by a symlink. The only safe way to refer to external
    // modules is to store them in a knwon location (ex. ~/cthun/).

    boost::filesystem::path module_path {
        boost::filesystem::canonical(
            boost::filesystem::system_complete(
                boost::filesystem::path(bin_path)).parent_path().parent_path())
    };
    module_path += "/modules";

    if (boost::filesystem::is_directory(module_path)) {
        boost::filesystem::directory_iterator end;

        for (auto file = boost::filesystem::directory_iterator(module_path);
                file != end; ++file) {
            if (!boost::filesystem::is_directory(file->status())) {
                LOG_INFO(file->path().string());

                try {
                    ExternalModule* external = new ExternalModule(file->path().string());
                    modules_[external->module_name] = std::shared_ptr<Module>(external);
                } catch (...) {
                    LOG_ERROR("failed to load: %1%", file->path().string());
                }
            }
        }
    } else {
        LOG_WARNING("failed to locate the modules directory; external modules "
                    "will not be loaded");
    }
}

AgentEndpoint::~AgentEndpoint() {
    if (ws_endpoint_ptr_) {
        // reset callbacks to avoid breaking the WebSocket Endpoint
        // with invalid reference context
        LOG_INFO("Resetting the WebSocket event callbacks");
        ws_endpoint_ptr_->resetCallbacks();
    }
}

void AgentEndpoint::startAgent(std::string url,
                               std::string ca_crt_path,
                               std::string client_crt_path,
                               std::string client_key_path) {
    listModules();
    ws_endpoint_ptr_.reset(new Cthun::WebSocket::Endpoint(url,
                                                          ca_crt_path,
                                                          client_crt_path,
                                                          client_key_path));
    setConnectionCallbacks();
    try {
        ws_endpoint_ptr_->connect();
    } catch (Cthun::WebSocket::connection_error& e) {
        LOG_WARNING(e.what());
        throw fatal_error { "failed to connect" };
    }
    monitorConnectionState();
}

//
// AgentEndpoint - private
//

void AgentEndpoint::listModules() {
    LOG_INFO("Loaded modules:");
    for (auto module : modules_) {
        LOG_INFO("   %1%", module.first);
        for (auto action : module.second->actions) {
            LOG_INFO("       %1%", action.first);
        }
    }
}

void AgentEndpoint::setConnectionCallbacks() {
    ws_endpoint_ptr_->setOnOpenCallback(
        [this]() {
            sendLogin();
        });

    ws_endpoint_ptr_->setOnMessageCallback(
        [this](std::string message) {
            processMessageAndSendResponse(message);
        });
}

void AgentEndpoint::sendLogin() {
    Message msg {};
    std::string login_id { Common::getUUID() };
    msg.set<std::string>(login_id, "id");
    msg.set<std::string>("1", "version");
    msg.set<std::string>(Common::StringUtils::getISO8601Time(
                            DEFAULT_MESSAGE_TIMEOUT_IN_SECONDS),
                         "expires");
    msg.set<std::string>(ws_endpoint_ptr_->identity(), "sender");
    std::vector<std::string> endpoints { "cth://server" };
    msg.set<std::vector<std::string>>(endpoints, "endpoints");
    std::vector<std::string> hops {};
    msg.set<std::vector<std::string>>(hops, "hops");
    msg.set<std::string>("http://puppetlabs.com/loginschema", "data_schema");
    msg.set<std::string>("agent", "data", "type");

    LOG_INFO("Sending login message with id: %1%", login_id);

    // NOTE(ploubser): I removed login schema message validation. We're making
    // it, it should never not be valid.
    LOG_DEBUG("Sending message - %1%", msg.toString());

    try {
         ws_endpoint_ptr_->send(msg.toString());
    }  catch(Cthun::WebSocket::message_error& e) {
        LOG_WARNING(e.what());
        throw e;
    }
}

Message AgentEndpoint::parseAndValidateMessage(std::string message) {
    Message msg { message };
    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!msg.validate(message_schema, errors)) {
        std::string error_message { "message schema validation failed:\n" };
        for (auto error : errors) {
            error_message += error + "\n";
        }

        throw validation_error { error_message };
    }

    if (std::string("http://puppetlabs.com/cncschema").compare(
            msg.get<std::string>("data_schema")) != 0) {
        throw validation_error { "message is not of cnc schema" };
    }

    valijson::Schema data_schema { Schemas::cnc_data() };
    if (!msg.validate(data_schema, errors, "data")) {
        std::string error_message { "data schema validation failed:\n" };
        for (auto error : errors) {
            error_message += error + "\n";
        }

        throw validation_error { error_message };
    }

    return msg;
}

void AgentEndpoint::sendResponse(std::string receiver_endpoint,
                                 std::string request_id,
                                 DataContainer output) {
    Message msg {};
    std::string response_id { Common::getUUID() };
    msg.set<std::string>(response_id, "id");
    msg.set<std::string>("1", "version");
    msg.set<std::string>(Common::StringUtils::getISO8601Time(
        DEFAULT_MESSAGE_TIMEOUT_IN_SECONDS), "expires");
    msg.set<std::string>(ws_endpoint_ptr_->identity(), "sender");
    std::vector<std::string> endpoints { receiver_endpoint };
    msg.set<std::vector<std::string>>(endpoints, "endpoints");
    std::vector<std::string> hops {};
    msg.set<std::vector<std::string>>(hops, "hops");
    msg.set<std::string>("http://puppetlabs.com/cncresponseschema", "data_schema");
    msg.set<DataContainer>(output, "data", "response");

    try {
        std::string response_txt = msg.toString();
        LOG_INFO("Responding to %1% with %2%.  Size %3%",
                 request_id, response_id, response_txt.size());
        LOG_DEBUG("Response:\n%1%", response_txt);
        ws_endpoint_ptr_->send(response_txt);
    }  catch(Cthun::WebSocket::message_error& e) {
        LOG_ERROR("Failed to send %1%: %2%", response_id, e.what());
    }
}

void AgentEndpoint::processMessageAndSendResponse(std::string message) {
    LOG_INFO("Received message:\n%1%", message);
    Message msg;
    Message response;

    try {
        msg = std::move(parseAndValidateMessage(message));
    } catch (validation_error& e) {
        LOG_ERROR("Invalid message: %1%", e.what());
        return;
    }

    std::string request_id = msg.get<std::string>("request_id");
    std::string module_name = msg.get<std::string>("data", "module");
    std::string action_name = msg.get<std::string>("data", "action");
    std::string sender_endpoint = msg.get<std::string>("sender");

    try {
        if (modules_.find(module_name) != modules_.end()) {
            std::shared_ptr<Module> module = modules_[module_name];

            if (module->actions.find(action_name) == module->actions.end()) {
                LOG_ERROR("Invalid request %1%: unknown action %2% for module %3%",
                          request_id, module_name, action_name);
                throw validation_error { "Invalid request: unknown action " +
                                         action_name + " for module " + module_name };
            }

            Agent::Action action = module->actions[action_name];

            if (action.behaviour.compare("delayed") == 0) {
                std::string uuid { Common::getUUID() };
                LOG_DEBUG("Delayed action execution requested. Creating job " \
                          "with ID %1%", uuid);
                DataContainer response {};
                response.set<std::string>("Requested excution of action: " + action_name,
                    "status");
                response.set<std::string>(uuid, "id");
                sendResponse(sender_endpoint, request_id, response);
                thread_queue_.push_back(std::thread(&AgentEndpoint::delayedActionThread,
                                                    this,
                                                    module,
                                                    action_name,
                                                    msg,
                                                    uuid));
            } else {
                DataContainer output { module->validate_and_call_action(action_name, msg) };
                LOG_DEBUG("Request %1%: '%2%' '%3%' output: %4%",
                          request_id, module_name, action_name,
                          output.toString());
                sendResponse(sender_endpoint, request_id, output);
            }
        } else {
            LOG_ERROR("Invalid request %1%: unknown module '%2%'",
                      request_id, module_name);
            throw validation_error { "Invalid request: unknown module " +
                                     module_name };
        }
    } catch (validation_error& e) {
        LOG_ERROR("Failed to perform %1%: '%2%' '%3%': %4%",
                  request_id, module_name, action_name, e.what());
        DataContainer err_result;
        err_result.set<std::string>(e.what(), "error");
        sendResponse(sender_endpoint, request_id, err_result);
    }
}

void AgentEndpoint::monitorConnectionState() {
    for (;;) {
        sleep(CONNECTION_STATE_CHECK_INTERVAL);

        if (ws_endpoint_ptr_->getConnectionState()
                != Cthun::WebSocket::ConnectionStateValues::open) {
            LOG_WARNING("Connection to Cthun server lost; retrying");
            ws_endpoint_ptr_->connect();
        } else {
            LOG_DEBUG("Sending heartbeat ping");
            ws_endpoint_ptr_->ping();
        }
    }
}

void AgentEndpoint::delayedActionThread(std::shared_ptr<Module> module,
                 std::string action_name,
                 Message msg,
                 std::string uuid) {
    // explicitly ignore return value
    (void) module->validate_and_call_action(action_name, msg, uuid);
}

}  // namespace Agent
}  // namespace Cthun
