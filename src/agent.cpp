#include "src/agent.h"
#include "src/modules/echo.h"
#include "src/modules/inventory.h"
#include "src/modules/ping.h"
#include "src/modules/status.h"
#include "src/external_module.h"
#include "src/schemas.h"
#include "src/errors.h"
#include "src/log.h"
#include "src/uuid.h"
#include "src/string_utils.h"
#include "src/websocket/errors.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <vector>
#include <memory>

LOG_DECLARE_NAMESPACE("agent");

namespace CthunAgent {

static const uint CONNECTION_STATE_CHECK_INTERVAL { 15 };
static const int DEFAULT_MSG_TIMEOUT_SEC { 10 };

Agent::Agent(std::string bin_path) {
    // declare internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);

    // TODO(ale): CTH-76 - this doesn't work if bin_path (argv[0]) has
    // only the name of the executable, neither when cthun_agent is
    // called by a symlink. The only safe way to refer to external
    // modules is to store them in a known location (ex. ~/cthun/).

    namespace fs = boost::filesystem;

    fs::path module_path {
        fs::canonical(
            fs::system_complete(
                fs::path(bin_path)).parent_path().parent_path())
    };
    module_path += "/modules";

    if (fs::is_directory(module_path)) {
        fs::directory_iterator end;

        for (auto f = fs::directory_iterator(module_path); f != end; ++f) {
            if (!fs::is_directory(f->status())) {
                auto f_p = f->path().string();
                LOG_INFO(f_p);

                try {
                    ExternalModule* external = new ExternalModule(f_p);
                    modules_[external->module_name] =
                        std::shared_ptr<Module>(external);
                } catch (std::exception& e) {
                    LOG_ERROR("Failed to load %1%; %2%", f_p, e.what());
                } catch (...) {
                    LOG_ERROR("Failed to load %1%; unexpected error", f_p);
                }
            }
        }
    } else {
        LOG_WARNING("Failed to locate the modules directory; external modules "
                    "will not be loaded");
    }
}

Agent::~Agent() {
    if (ws_endpoint_ptr_) {
        // reset callbacks to avoid breaking the WebSocket Endpoint
        // with invalid reference context
        LOG_INFO("Resetting the WebSocket event callbacks");
        ws_endpoint_ptr_->resetCallbacks();
    }
}

void Agent::startAgent(std::string url,
                       std::string ca_crt_path,
                       std::string client_crt_path,
                       std::string client_key_path) {
    listModules();

    try {
        ws_endpoint_ptr_.reset(new WebSocket::Endpoint(url,
                                                       ca_crt_path,
                                                       client_crt_path,
                                                       client_key_path));
    } catch (WebSocket::websocket_error& e) {
        LOG_WARNING(e.what());
        throw fatal_error { "failed to initialize" };
    }

    setConnectionCallbacks();

    try {
        ws_endpoint_ptr_->connect();
    } catch (WebSocket::connection_error& e) {
        LOG_WARNING(e.what());
        throw fatal_error { "failed to connect" };
    }
    monitorConnectionState();
}

//
// Agent - private
//

void Agent::listModules() {
    LOG_INFO("Loaded modules:");
    for (auto module : modules_) {
        LOG_INFO("   %1%", module.first);
        for (auto action : module.second->actions) {
            LOG_INFO("       %1%", action.first);
        }
    }
}

void Agent::setConnectionCallbacks() {
    ws_endpoint_ptr_->setOnOpenCallback(
        [this]() {
            sendLogin();
        });

    ws_endpoint_ptr_->setOnMessageCallback(
        [this](std::string message) {
            processMessageAndSendResponse(message);
        });
}

void Agent::addCommonEnvelopeEntries(DataContainer& envelope_entries) {
    auto id = UUID::getUUID();
    auto expires = StringUtils::getISO8601Time(DEFAULT_MSG_TIMEOUT_SEC);

    envelope_entries.set<std::string>(id, "id");
    envelope_entries.set<std::string>(expires, "expires");
    envelope_entries.set<std::string>(ws_endpoint_ptr_->identity(), "sender");
}

// NB: WebSocket onOpen callback
void Agent::sendLogin() {
    // Envelope
    DataContainer envelope_entries {};
    addCommonEnvelopeEntries(envelope_entries);
    std::vector<std::string> endpoints { "cth://server" };

    envelope_entries.set<std::vector<std::string>>(endpoints, "endpoints");
    envelope_entries.set<std::string>("http://puppetlabs.com/loginschema",
                                      "data_schema");

    // Data
    DataContainer data_entries {};
    data_entries.set<std::string>("agent", "type");

    // Create message chunks
    MessageChunk envelope { ChunkDescriptor::ENVELOPE,
                            envelope_entries.toString() };
    MessageChunk data { ChunkDescriptor::DATA, data_entries.toString() };

    try {
        Message msg { envelope };
        msg.setDataChunk(data);
        auto serialized_msg = msg.getSerialized();

        LOG_INFO("Sending login message with id: %1%",
                 envelope_entries.get<std::string>("id"));
        LOG_DEBUG("Login message data: %1%", data.content);

        ws_endpoint_ptr_->send(&serialized_msg[0], serialized_msg.size());
    }  catch(WebSocket::message_error& e) {
        LOG_WARNING(e.what());
        throw e;
    }
}

void Agent::sendResponse(std::string receiver_endpoint,
                         std::string request_id,
                         DataContainer response_output,
                         std::vector<MessageChunk> debug_chunks) {
    // Envelope
    DataContainer envelope_entries {};
    addCommonEnvelopeEntries(envelope_entries);
    std::vector<std::string> endpoints { receiver_endpoint };

    envelope_entries.set<std::vector<std::string>>(endpoints, "endpoints");
    envelope_entries.set<std::string>("http://puppetlabs.com/cncresponseschema",
                                      "data_schema");

    // Data
    DataContainer data_entries {};
    data_entries.set<DataContainer>(response_output, "response");

    // Create message chunks (debug chunks copied from the reponse)
    MessageChunk envelope { ChunkDescriptor::ENVELOPE,
                            envelope_entries.toString() };
    MessageChunk data { ChunkDescriptor::DATA, data_entries.toString() };

    try {
        Message msg { envelope };
        msg.setDataChunk(data);
        for (const auto& d_c : debug_chunks) {
            msg.addDebugChunk(d_c);
        }
        auto serialized_msg = msg.getSerialized();

        LOG_INFO("Responding to request %1%; response %2%, size %3% bytes",
                  request_id, envelope_entries.get<std::string>("id"),
                  serialized_msg.size());
        LOG_DEBUG("Response %1%:\n%2%",
                  envelope_entries.get<std::string>("id"), msg.toString());

        ws_endpoint_ptr_->send(&serialized_msg[0], serialized_msg.size());
    }  catch(WebSocket::message_error& e) {
        LOG_ERROR("Failed to send %1%: %2%",
                  envelope_entries.get<std::string>("id"), e.what());
    }
}

// NB: WebSocket onMessage callback
void Agent::processMessageAndSendResponse(std::string message_txt) {
    LOG_INFO("Received message:\n%1%", message_txt);

    // Serialize the incoming message
    std::unique_ptr<Message> msg_ptr;
    try {
        msg_ptr.reset(new Message(message_txt));
    } catch (message_processing_error& e) {
        LOG_ERROR("Failed to deserialize message: %1%", e.what());
        return;
    }

    // Parse the message content
    ParsedContent parsed_content;
    try {
        parsed_content = msg_ptr->getParsedContent();
    } catch (message_validation_error& e) {
        LOG_ERROR("Invalid message: %1%", e.what());
        return;
    }

    auto request_id = parsed_content.envelope.get<std::string>("id");
    auto sender_endpoint = parsed_content.envelope.get<std::string>("sender");

    auto module_name = parsed_content.data.get<std::string>("module");
    auto action_name = parsed_content.data.get<std::string>("action");

    try {
        if (modules_.find(module_name) != modules_.end()) {
            std::shared_ptr<Module> module_ptr = modules_[module_name];

            // Finally, validate and execute the requested action
            auto output = module_ptr->validateAndCallAction(action_name,
                                                            parsed_content);

            sendResponse(sender_endpoint,
                         request_id,
                         output,
                         msg_ptr->getDebugChunks());
        } else {
            throw message_validation_error { "unknown module " + module_name };
        }
    } catch (message_error& e) {
        LOG_ERROR("Failed to perform '%1% %2%' for request %3%: %4%",
                  module_name, action_name, request_id, e.what());
        DataContainer err_result;
        err_result.set<std::string>(e.what(), "error");
        sendResponse(sender_endpoint,
                     request_id,
                     err_result,
                     msg_ptr->getDebugChunks());
    }
}

void Agent::monitorConnectionState() {
    for (;;) {
        sleep(CONNECTION_STATE_CHECK_INTERVAL);

        if (ws_endpoint_ptr_->getConnectionState()
                != WebSocket::ConnectionStateValues::open) {
            LOG_WARNING("Connection to Cthun server lost; retrying");
            ws_endpoint_ptr_->connect();
        } else {
            LOG_DEBUG("Sending heartbeat ping");
            ws_endpoint_ptr_->ping();
        }
    }
}

}  // namespace CthunAgent
