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

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <string>

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

LOG_DECLARE_NAMESPACE("agent.agent_endpoint");

namespace Cthun {
namespace Agent {

//
// Tokens
//

static const uint DEFAULT_HEARTBEAT_PERIOD { 30 };  // [s]
static const uint BACKOFF_MULTIPLIER { 2 };
static const uint BACKOFF_LIMIT { 30 };
static const uint CONNECTION_STATE_CHECK_INTERVAL { 10 };

//
// HeartbeatTask
//

HeartbeatTask::HeartbeatTask(Cthun::WebSocket::Connection::Ptr connection_ptr)
        : must_stop_ { false } {
    assert(connection_ptr != nullptr);
    connection_ptr_ = connection_ptr;
}

HeartbeatTask::~HeartbeatTask() {
    stop();
}

void HeartbeatTask::start() {
    LOG_INFO("starting the heartbeat task");
    heartbeat_thread_ = std::thread(&HeartbeatTask::heartbeatThread, this);
}

void HeartbeatTask::stop() {
    LOG_INFO("stopping the heartbeat task");
    must_stop_ = true;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
}

void HeartbeatTask::heartbeatThread() {
    while (!must_stop_) {
        try {
            if (connection_ptr_->getState() == Cthun::WebSocket::Connection_State_Values::open) {
                Cthun::WebSocket::CONNECTION_MANAGER.ping(connection_ptr_,
                                                          binary_payload_);
            } else {
                LOG_DEBUG("skipping ping; connection is not open");
            }
        } catch (Cthun::WebSocket::message_error& e) {
            LOG_ERROR(e.what());
        }
        sleep(DEFAULT_HEARTBEAT_PERIOD);
    }
}

//
// AgentEndpoint
//

AgentEndpoint::AgentEndpoint() {
    // declare internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);


    // load external modules
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
    if (connection_ptr_ != nullptr) {
        // reset callbacks to avoid breaking the WebSocket Endpoint
        // with invalid references

        LOG_INFO("resetting the WebSocket event callbacks");
        connection_ptr_->resetCallbacks();

        // NB: the heartbeat task will be destructed by RIIA
    }
}

void AgentEndpoint::startAgent(std::string url,
                               std::string ca_crt_path,
                               std::string client_crt_path,
                               std::string client_key_path) {
    listModules();

    // Configure the secure WebSocket endpoint

    try {
        Cthun::WebSocket::CONNECTION_MANAGER.configureSecureEndpoint(ca_crt_path,
                                                                     client_crt_path,
                                                                     client_key_path);
    } catch (Cthun::WebSocket::endpoint_error& e) {
        LOG_WARNING("failed to configure the WebSocket endpoint: %1%", e.what());
        throw fatal_error("failed to configure the WebSocket endpoint");
    }

    // Configure the connection

    connection_ptr_ = Cthun::WebSocket::CONNECTION_MANAGER.createConnection(url);
    setConnectionCallbacks();

    // Connect and wait for open connection event

    try {
        Cthun::WebSocket::CONNECTION_MANAGER.open(connection_ptr_);
        connection_ptr_->waitForOpen();
    } catch (Cthun::WebSocket::connection_error& e) {
        LOG_WARNING("failed to connect; %1%", e.what());
        throw fatal_error { "failed to connect" };
    }

    // Start heartbeat task

    HeartbeatTask heartbeat_task { connection_ptr_ };
    heartbeat_task.start();

    // Keep monitoring the connection

    monitorConnectionState();
}

//
// AgentEndpoint - private
//

void AgentEndpoint::listModules() {
    LOG_INFO("loaded modules:");
    for (auto module : modules_) {
        LOG_INFO("   %1%", module.first);
        for (auto action : module.second->actions) {
            LOG_INFO("       %1%", action.first);
        }
    }
}

void AgentEndpoint::sendLogin(Cthun::WebSocket::Client_Type* client_ptr) {
    Json::Value login {};
    login["id"] = 1;
    login["version"] = "1";
    login["expires"] = "2014-08-28T17:01:05Z";
    login["sender"] = "cth://localhost/agent";
    login["endpoints"] = Json::Value { Json::arrayValue };
    login["endpoints"][0] = "cth://server";
    login["hops"] = Json::Value { Json::arrayValue };
    login["data_schema"] = "http://puppetlabs.com/loginschema";
    login["data"]["type"] = "agent";


    LOG_INFO("login message:\n%1%", login.toStyledString());

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(login, message_schema, errors)) {
        LOG_WARNING("validation failed");
        for (auto error : errors) {
            LOG_WARNING("    %1%", error);
        }
        // This is unexpected
        throw fatal_error { "invalid login message schema" };
    }

    auto handle = connection_ptr_->getConnectionHandle();
    try {
        client_ptr->send(handle, login.toStyledString(),
                         Cthun::WebSocket::Frame_Opcode_Values::text);
    }  catch(Cthun::WebSocket::message_error& e) {
        LOG_WARNING("failed to send: %1%", e.what());
        // Fatal; we can't login...
        throw fatal_error { "failed to send login message" };
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
        // TODO(ale): refactor error logging
        LOG_WARNING("data schema validation failed; logging the errors");
        for (auto error : errors) {
            LOG_WARNING("    %1%", error);
        }
        throw validation_error { "data schema validation failed" };
    }

    return doc;
}

void AgentEndpoint::handleMessage(Cthun::WebSocket::Client_Type* client_ptr,
                                   std::string message) {
    LOG_INFO("received message:\n%1%", message);

    Json::Value doc;

    try {
        doc = parseAndValidateMessage(message);
    } catch (validation_error& e) {
        LOG_ERROR("invalid message: %1%", e.what());
        return;
    }

    Json::Value output;
    std::string module_name = doc["data"]["module"].asString();
    std::string action_name = doc["data"]["action"].asString();
    std::string sender = doc["sender"].asString();

    try {
        if (modules_.find(module_name) != modules_.end()) {
            std::shared_ptr<Module> module = modules_[module_name];

            if (module->actions.find(action_name) == module->actions.end()) {
                LOG_ERROR("Invalid request: unknown action %1% for module %2%",
                          module_name, action_name);
                throw validation_error { "Invalid request: unknown action " +
                                         action_name +
                                         " for module " +
                                         module_name };
            }
            Agent::Action action = module->actions[action_name];


            if (action.behaviour.compare("delayed") == 0) {
            // Create uid for task
                std::string uuid { Common::getUUID() };
                LOG_DEBUG("Delayed action execution requested. Creating job with ID %1%",
                          uuid);
                Json::Value output {};
                output["status"] = "Requested excution of action: " + action_name;
                output["id"] = uuid;
                sendResponseMessage(sender, output, client_ptr);
                LOG_ERROR("DONE SENDING...");
                module->validate_and_call_action(action_name,
                                                 doc["data"]["params"],
                                                 output,
                                                 uuid);
            } else {
                module->validate_and_call_action(action_name,
                                                 doc["data"]["params"],
                                                 output);
                LOG_DEBUG("%1% %2% output: %3%",
                          module_name, action_name, output.toStyledString());
                sendResponseMessage(sender, output, client_ptr);
            }
        } else {
            LOG_ERROR("Invalid request: unknown module %1%", module_name);
            throw validation_error { "Invalid request: unknown module " +
                                     module_name };
        }
    } catch (validation_error& e) {
        LOG_ERROR("failed to perform '%1% %2%': %3%",
                  module_name, action_name, e.what());
        Json::Value err_result;
        err_result["error"] = e.what();
        sendResponseMessage(sender, err_result, client_ptr);
    }
}

void AgentEndpoint::sendResponseMessage(std::string sender,
                                        Json::Value output,
                                        Cthun::WebSocket::Client_Type* client_ptr) {
    Json::Value body {};
    body["id"] = 2;
    body["version"] = "1";
    body["expires"] = "2014-08-28T17:01:05Z";
    body["sender"] = "cth://localhost/agent";
    body["endpoints"] = Json::Value { Json::arrayValue };
    body["endpoints"][0] = sender;
    body["hops"] = Json::Value { Json::arrayValue };
    body["data_schema"] = "http://puppetlabs.com/cncresponseschema";
    body["data"]["response"] = output;

    try {
        std::string response_txt = body.toStyledString();
        LOG_INFO("sending response of size %1%", response_txt.size());
        LOG_DEBUG("response:\n%1%", response_txt);

        auto handle = connection_ptr_->getConnectionHandle();
        client_ptr->send(handle,
                         response_txt,
                         Cthun::WebSocket::Frame_Opcode_Values::text);
    }  catch(Cthun::WebSocket::message_error& e) {
        LOG_ERROR("failed to send: %1%", e.what());
        // we don't want to throw anything here
    } catch (std::exception&  e) {
        LOG_ERROR("unexpected exception: %1%", e.what());
    } catch (...) {
        LOG_ERROR("badness occured");
    }
}

void AgentEndpoint::setConnectionCallbacks() {
    connection_ptr_->setOnOpenCallback(
        [this](Cthun::WebSocket::Client_Type* client_ptr,
               Cthun::WebSocket::Connection::Ptr connection_ptr_c) {
            assert(connection_ptr_ == connection_ptr_c);
            sendLogin(client_ptr);
        });

    connection_ptr_->setOnMessageCallback(
        [this](Cthun::WebSocket::Client_Type* client_ptr,
               Cthun::WebSocket::Connection::Ptr connection_ptr_c,
               std::string message) {
            assert(connection_ptr_ == connection_ptr_c);
            handleMessage(client_ptr, message);
        });

    std::atomic<uint> consecutive_pong_timeouts { 0 };

    connection_ptr_->setOnPongCallback(
        [&consecutive_pong_timeouts](Cthun::WebSocket::Client_Type* client_ptr,
                                     Cthun::WebSocket::Connection::Ptr connection_ptr_c,
                                     std::string binary_payload) {
            LOG_DEBUG("received pong - payload: '%1%'", binary_payload);
            if (consecutive_pong_timeouts > 0){
                consecutive_pong_timeouts = 0;
            }
        });

    connection_ptr_->setOnPongTimeoutCallback(
        [&consecutive_pong_timeouts](Cthun::WebSocket::Client_Type* client_ptr,
                                     Cthun::WebSocket::Connection::Ptr connection_ptr_c,
                                     std::string binary_payload) {
            LOG_WARNING("pong timeout (%1% consecutive) - payload: '%2 %'",
                        std::to_string(consecutive_pong_timeouts), binary_payload);
            ++consecutive_pong_timeouts;
        });
}

void AgentEndpoint::monitorConnectionState() {
    uint backoff_seconds = 2;
    for (;;) {
        sleep(CONNECTION_STATE_CHECK_INTERVAL);

        if (connection_ptr_->getState()
                != Cthun::WebSocket::Connection_State_Values::open) {
            LOG_WARNING("Connection to Cthun server lost. Retrying.");
            // Attempt to re-establish connection
            while (connection_ptr_->getState()
                != Cthun::WebSocket::Connection_State_Values::open) {
                try {
                    Cthun::WebSocket::CONNECTION_MANAGER.open(connection_ptr_);
                    connection_ptr_->waitForOpen();
                    LOG_INFO("Successfully re-established connection to Cthun server.");
                } catch (Cthun::WebSocket::connection_error& e) {
                    LOG_WARNING("Failed to re-establish connection to Cthun server");
                    LOG_INFO("Attemping reconnect in %1% seconds.", backoff_seconds);
                    sleep(backoff_seconds);
                    if ((backoff_seconds *= BACKOFF_MULTIPLIER) >= BACKOFF_LIMIT) {
                        // Limit backoff to 30 seconds.
                        backoff_seconds = 30;
                    }
                }
            }
        }
    }
}

}  // namespace Agent
}  // namespace Cthun
