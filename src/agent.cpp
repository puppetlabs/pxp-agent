#include "src/agent.h"
#include "src/modules/echo.h"
#include "src/modules/inventory.h"
#include "src/modules/ping.h"
#include "src/modules/status.h"
#include "src/external_module.h"
#include "src/schemas.h"
#include "src/errors.h"
#include "src/uuid.h"
#include "src/string_utils.h"
#include "src/websocket/errors.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.agent"
#include <leatherman/logging/logging.hpp>


#include <vector>
#include <memory>

namespace CthunAgent {

namespace fs = boost::filesystem;

static const uint CONNECTION_STATE_CHECK_INTERVAL { 15 };
static const int DEFAULT_MSG_TIMEOUT_SEC { 10 };

//
// Utility functions
//

std::string getAgentIdentityFromCert_(const std::string& client_crt_path) {
    // TODO(ale): fix compiler warnings about deprecated openssl types

    std::unique_ptr<std::FILE, int(*)(std::FILE*)> fp {
        std::fopen(client_crt_path.data(), "r"), std::fclose };
    if (fp == nullptr) {
        throw fatal_error { "certificate file '" + client_crt_path
                            + "' does not exist." };
    }
    std::unique_ptr<X509, void(*)(X509*)> cert {
        PEM_read_X509(fp.get(), NULL, NULL, NULL), X509_free };
    X509_NAME* subj = X509_get_subject_name(cert.get());
    X509_NAME_ENTRY* entry = X509_NAME_get_entry(subj, 0);
    ASN1_STRING* asn1_string = X509_NAME_ENTRY_get_data(entry);
    unsigned char* tmp = ASN1_STRING_data(asn1_string);
    int string_size = ASN1_STRING_length(asn1_string);
    return "cth://" + std::string(tmp, tmp + string_size) + "/cthun-agent";
}

//
// Agent
//

Agent::Agent(std::string bin_path,
             std::string ca_crt_path,
             std::string client_crt_path,
             std::string client_key_path)
        : ca_crt_path_ { ca_crt_path },
          client_crt_path_ { client_crt_path },
          client_key_path_ { client_key_path },
          identity_ { getAgentIdentityFromCert_(client_crt_path) } {
    // NB: certificate paths are validated by HW

    // declare internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["inventory"] = std::shared_ptr<Module>(new Modules::Inventory);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);

    // TODO(ale): CTH-76 - this doesn't work if bin_path (argv[0]) has
    // only the name of the executable, neither when cthun_agent is
    // called by a symlink. The only safe way to refer to external
    // modules is to store them in a known location (ex. ~/cthun/).

    fs::path modules_dir_path {
        fs::canonical(
            fs::system_complete(
                fs::path(bin_path)).parent_path().parent_path())
    };
    modules_dir_path += "/modules";
    loadExternalModules_(modules_dir_path);

    listModules_();
}

Agent::~Agent() {
    if (ws_endpoint_ptr_) {
        // reset callbacks to avoid breaking the WebSocket Endpoint
        // with invalid reference context
        LOG_INFO("Resetting the WebSocket event callbacks");
        ws_endpoint_ptr_->resetCallbacks();
    }
}

void Agent::startAgent(std::string server_url) {
    try {
        ws_endpoint_ptr_.reset(new WebSocket::Endpoint(server_url,
                                                       ca_crt_path_,
                                                       client_crt_path_,
                                                       client_key_path_));
    } catch (WebSocket::websocket_error& e) {
        LOG_WARNING(e.what());
        throw fatal_error { "failed to initialize" };
    }

    setConnectionCallbacks_();

    try {
        ws_endpoint_ptr_->connect();
    } catch (WebSocket::connection_error& e) {
        LOG_WARNING(e.what());
        throw fatal_error { "failed to connect" };
    }
    monitorConnectionState_();
}

std::string Agent::getIdentity() const {
    return identity_;
}

//
// Agent - private interface
//

void Agent::loadExternalModules_(boost::filesystem::path modules_dir_path) {
    if (fs::is_directory(modules_dir_path)) {
        fs::directory_iterator end;

        for (auto f = fs::directory_iterator(modules_dir_path); f != end; ++f) {
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

void Agent::listModules_() const {
    LOG_INFO("Loaded modules:");
    for (auto module : modules_) {
        LOG_INFO("   %1%", module.first);
        for (auto action : module.second->actions) {
            LOG_INFO("       %1%", action.first);
        }
    }
}

void Agent::setConnectionCallbacks_() {
    ws_endpoint_ptr_->setOnOpenCallback(
        [this]() {
            sendLogin_();
        });

    ws_endpoint_ptr_->setOnMessageCallback(
        [this](std::string message) {
            processMessageAndSendResponse_(message);
        });
}

void Agent::addCommonEnvelopeEntries_(DataContainer& envelope_entries) {
    auto msg_id = UUID::getUUID();
    auto expires = StringUtils::getISO8601Time(DEFAULT_MSG_TIMEOUT_SEC);

    envelope_entries.set<std::string>(msg_id, "id");
    envelope_entries.set<std::string>(expires, "expires");
    envelope_entries.set<std::string>(identity_, "sender");
}

// NB: WebSocket onOpen callback
void Agent::sendLogin_() {
    // Envelope
    DataContainer envelope_entries {};
    addCommonEnvelopeEntries_(envelope_entries);
    std::vector<std::string> endpoints { "cth://server" };

    envelope_entries.set<std::vector<std::string>>(endpoints, "endpoints");
    envelope_entries.set<std::string>(CTHUN_LOGIN_SCHEMA, "data_schema");

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

void Agent::sendResponse_(std::string receiver_endpoint,
                          std::string request_id,
                          DataContainer response_output,
                          std::vector<MessageChunk> debug_chunks) {
    // Envelope
    DataContainer envelope_entries {};
    addCommonEnvelopeEntries_(envelope_entries);
    std::vector<std::string> endpoints { receiver_endpoint };

    envelope_entries.set<std::vector<std::string>>(endpoints, "endpoints");
    envelope_entries.set<std::string>(CTHUN_RESPONSE_SCHEMA, "data_schema");

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

        // Copy debug chunks
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
void Agent::processMessageAndSendResponse_(std::string message_txt) {
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

            sendResponse_(sender_endpoint,
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
        sendResponse_(sender_endpoint,
                      request_id,
                      err_result,
                      msg_ptr->getDebugChunks());
    }
}

void Agent::monitorConnectionState_() {
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
