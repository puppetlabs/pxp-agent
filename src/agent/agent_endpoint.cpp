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

AgentEndpoint::AgentEndpoint() {
    // declare internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);

    // load external modules
    // TODO(ale): fix; this breaks if cthun-agent is not invoked from root dir
    boost::filesystem::path module_path { "modules" };
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
    Json::Value login {};
    login["id"] = Common::getUUID();
    login["version"] = "1";
    login["expires"] = Common::StringUtils::getISO8601Time(
        DEFAULT_MESSAGE_TIMEOUT_IN_SECONDS);
    login["sender"] = "cth://localhost/agent";
    login["endpoints"] = Json::Value { Json::arrayValue };
    login["endpoints"][0] = "cth://server";
    login["hops"] = Json::Value { Json::arrayValue };
    login["data_schema"] = "http://puppetlabs.com/loginschema";
    login["data"]["type"] = "agent";

    LOG_INFO("Sending login message with id: %1%", login["id"].asString());

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(login, message_schema, errors)) {
        LOG_WARNING("Validation failed");
        for (auto error : errors) {
            LOG_WARNING("    %1%", error);
        }
        // This is unexpected since we're sending the above message
        throw Cthun::WebSocket::message_error { "invalid login message schema" };
    }

    try {
         ws_endpoint_ptr_->send(login.toStyledString());
    }  catch(Cthun::WebSocket::message_error& e) {
        LOG_WARNING(e.what());
        throw e;
    }
}

Json::Value AgentEndpoint::parseAndValidateMessage(std::string message) {
    Json::Value doc;
    Json::Reader reader;

    if (!reader.parse(message, doc)) {
        throw validation_error { "json decode of message failed" };
    }

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(doc, message_schema, errors)) {
        throw validation_error { "message schema validation failed" };
    }

    if (std::string("http://puppetlabs.com/cncschema").compare(
            doc["data_schema"].asString()) != 0) {
        throw validation_error { "message is not of cnc schema" };
    }

    valijson::Schema data_schema { Schemas::cnc_data() };
    if (!Schemas::validate(doc["data"], data_schema, errors)) {
        LOG_WARNING("Data schema validation failed; logging the errors");
        for (auto error : errors) {
            LOG_WARNING("    %1%", error);
        }
        throw validation_error { "data schema validation failed" };
    }

    return doc;
}

void AgentEndpoint::sendResponse(std::string receiver_endpoint,
                                 std::string request_id,
                                 Json::Value output) {
    Json::Value body {};
    std::string response_id { Common::getUUID() };
    body["id"] = response_id;
    body["version"] = "1";
    body["expires"] = Common::StringUtils::getISO8601Time(
        DEFAULT_MESSAGE_TIMEOUT_IN_SECONDS);
    body["sender"] = "cth://localhost/agent";
    body["endpoints"] = Json::Value { Json::arrayValue };
    body["endpoints"][0] = receiver_endpoint;
    body["hops"] = Json::Value { Json::arrayValue };
    body["data_schema"] = "http://puppetlabs.com/cncresponseschema";
    body["data"]["response"] = output;

    try {
        std::string response_txt = body.toStyledString();
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
    Json::Value doc;

    try {
        doc = parseAndValidateMessage(message);
    } catch (validation_error& e) {
        LOG_ERROR("Invalid message: %1%", e.what());
        return;
    }

    Json::Value output;
    std::string request_id = doc["id"].asString();
    std::string module_name = doc["data"]["module"].asString();
    std::string action_name = doc["data"]["action"].asString();
    std::string sender_endpoint = doc["sender"].asString();

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
                Json::Value output {};
                output["status"] = "Requested excution of action: " + action_name;
                output["id"] = uuid;
                sendResponse(sender_endpoint, request_id, output);
                thread_queue_.push_back(std::thread(&AgentEndpoint::delayedActionThread,
                                                    this,
                                                    module,
                                                    action_name,
                                                    doc,
                                                    output,
                                                    uuid));
            } else {
                module->validate_and_call_action(action_name,
                                                 doc,
                                                 doc["data"]["params"],
                                                 output);
                LOG_DEBUG("Request %1%: '%2%' '%3%' output: %4%",
                          request_id, module_name, action_name,
                          output.toStyledString());
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
        Json::Value err_result;
        err_result["error"] = e.what();
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
                 Json::Value doc,
                 Json::Value output,
                 std::string uuid) {
    module->validate_and_call_action(action_name,
                                     doc,
                                     doc["data"]["params"],
                                     output,
                                     uuid);
}

}  // namespace Agent
}  // namespace Cthun
