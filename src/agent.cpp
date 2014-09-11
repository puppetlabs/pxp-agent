#include "agent.h"
#include "modules/echo.h"
#include "external_module.h"
#include "schemas.h"

#include <cthun-client/src/log/log.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

LOG_DECLARE_NAMESPACE("agent.agent");

namespace CthunAgent {

Agent::Agent() {
    // declare internal modules
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);

    // load external modules
    boost::filesystem::path module_path { "modules" };
    boost::filesystem::directory_iterator end;

    for (auto file = boost::filesystem::directory_iterator(module_path); file != end; ++file) {
        if (!boost::filesystem::is_directory(file->status())) {
            LOG_INFO(file->path().string());

            try {
                ExternalModule* external = new ExternalModule(file->path().string());
                modules_[external->name] = std::shared_ptr<Module>(external);
            } catch (...) {
                LOG_ERROR("failed to load: %1%", file->path().string());
            }
        }
    }
}

Agent::~Agent() {
    if (connection_ptr_ != nullptr) {
        // don't break the WebSocket Endpoint with our "[this]" callbacks
        LOG_INFO("resetting the event callbacks");
        Cthun::Client::Connection::Event_Callback onOpen_c =
            [](Cthun::Client::Client_Type* client_ptr,
               Cthun::Client::Connection::Ptr connection_ptr) {};

        Cthun::Client::Connection::OnMessage_Callback onMessage_c =
            [](Cthun::Client::Client_Type* client_ptr,
               Cthun::Client::Connection::Ptr connection_ptr, std::string message) {};

        connection_ptr_->setOnOpenCallback(onOpen_c);
        connection_ptr_->setOnMessageCallback(onMessage_c);
    }
}

void Agent::list_modules() {
    LOG_INFO("loaded modules:");
    for (auto module : modules_) {
        LOG_INFO("   %1%", module.first);
        for (auto action : module.second->actions) {
            LOG_INFO("       %1%", action.first);
        }
    }
}

void Agent::run(std::string module, std::string action) {
    list_modules();

    std::shared_ptr<Module> the_module = modules_[module];

    Json::Reader reader;
    Json::Value input;

    LOG_INFO("loading stdin");

    if (!reader.parse(std::cin, input)) {
        LOG_ERROR("parse error: %1%", reader.getFormatedErrorMessages());
        return;
    }

    try {
        Json::Value output;
        the_module->validate_and_call_action(action, input, output);
        LOG_INFO(output.toStyledString());
    } catch (...) {
        LOG_ERROR("badness occured");
    }
}

void Agent::send_login(Cthun::Client::Client_Type* client_ptr) {
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
    login["data"]["user"] = "agent";


    LOG_INFO("login message:\n%1%", login.toStyledString());

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(login, message_schema, errors)) {
        LOG_ERROR("validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        // TODO(ale): use specific errors
        throw "input schema mismatch";
    }

    auto handle = connection_ptr_->getConnectionHandle();
    try {
        client_ptr->send(handle, login.toStyledString(),
                         Cthun::Client::Frame_Opcode_Values::text);
    }  catch(Cthun::Client::message_error& e) {
        LOG_ERROR("failed to send: %1%", e.what());
        // TODO(ale): is it correct to rethrow? Any cleanup?
        throw;
    }
}

void Agent::handle_message(Cthun::Client::Client_Type* client_ptr,
                           std::string message) {
    LOG_INFO("received message:\n%1%", message);

    Json::Value document;
    Json::Reader reader;

    if (!reader.parse(message, document)) {
        LOG_ERROR("json decode of message failed");
        return;
    }

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(document, message_schema, errors)) {
        LOG_ERROR("message schema validation failed");
        return;
    }

    if (std::string("http://puppetlabs.com/cncschema").compare(
            document["data_schema"].asString()) != 0) {
        LOG_ERROR("message is not of cnc schema");
        return;
    }

    valijson::Schema data_schema { Schemas::cnc_data() };
    if (!Schemas::validate(document["data"], data_schema, errors)) {
        // TODO(ale): refactor error logging
        LOG_ERROR("data schema validation failed");
        for (auto error : errors) {
            LOG_ERROR("    %1%", error);
        }
        return;
    }

    std::shared_ptr<Module> module =
        modules_[document["data"]["module"].asString()];

    try {
        Json::Value output;
        module->validate_and_call_action(document["data"]["action"].asString(),
                                         document["data"]["params"],
                                         output);
        LOG_INFO(output.toStyledString());

        Json::Value response {};
        response["id"] = 2;
        response["version"] = "1";
        response["expires"] = "2014-08-28T17:01:05Z";
        response["sender"] = "cth://localhost/agent";
        response["endpoints"] = Json::Value { Json::arrayValue };
        response["endpoints"][0] = document["sender"];
        response["hops"] = Json::Value { Json::arrayValue };
        response["data_schema"] = "http://puppetlabs.com/cncresponseschema";
        response["data"]["response"] = output;

        LOG_INFO("sending response %1%", response.toStyledString());
        auto handle = connection_ptr_->getConnectionHandle();
        client_ptr->send(handle, response.toStyledString(),
                         Cthun::Client::Frame_Opcode_Values::text);
    }  catch(Cthun::Client::message_error& e) {
        LOG_ERROR("failed to send: %1%", e.what());
        // TODO(ale): check, as above; we don't want to throw here
    } catch (...) {
        LOG_ERROR("badness occured");
    }
}

void Agent::connect_and_run(std::string url,
                            std::string ca_crt_path,
                            std::string client_crt_path,
                            std::string client_key_path) {
    Cthun::Client::CONNECTION_MANAGER.configureSecureEndpoint(
        DEFAULT_CA, DEFAULT_CERT, DEFAULT_KEY);
    connection_ptr_ = Cthun::Client::CONNECTION_MANAGER.createConnection(url);

    Cthun::Client::Connection::Event_Callback onOpen_c =
        [this](Cthun::Client::Client_Type* client_ptr,
               Cthun::Client::Connection::Ptr connection_ptr_c) {
        assert(connection_ptr_ == connection_ptr_c);
        send_login(client_ptr);
    };

    Cthun::Client::Connection::OnMessage_Callback onMessage_c =
        [this](Cthun::Client::Client_Type* client_ptr,
               Cthun::Client::Connection::Ptr connection_ptr_c, std::string message) {
        assert(connection_ptr_ == connection_ptr_c);
        handle_message(client_ptr, message);
    };

    connection_ptr_->setOnOpenCallback(onOpen_c);
    connection_ptr_->setOnMessageCallback(onMessage_c);

    try {
        Cthun::Client::CONNECTION_MANAGER.open(connection_ptr_);
    } catch(Cthun::Client::connection_error& e) {
        LOG_ERROR("failed to connect: %1%", e.what());
        // TODO(ale): is it correct to rethrow? Any cleanup?
        throw;
    }

    while(1) {
        sleep(10);
    }
}

}  // namespace CthunAgent
