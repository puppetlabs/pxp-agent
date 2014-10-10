#include "src/websocket/connection.h"
#include "src/websocket/errors.h"
#include "src/common/uuid.h"
#include "src/common/log.h"

#include <string>
#include <iostream>

LOG_DECLARE_NAMESPACE("websocket.connection");

namespace Cthun {
namespace WebSocket {

Connection::Connection(const std::string& url)
    : url_ { url },
      id_ { Common::getUUID() },
      state_ { Connection_State_Values::connecting },
      remote_server_ { "N/A" } {
    // TODO(ale): validate url
}

void Connection::waitForOpen(int max_num_checks, int interval_check) {
    int idx { 0 };
    do {
        if (state_ == Cthun::WebSocket::Connection_State_Values::open) {
            return;
        }
        idx++;
        sleep(interval_check);
    } while (idx < max_num_checks);
    throw connection_error { "on open event timeout (waited for "
                             + std::to_string(max_num_checks * interval_check)
                             + " s)" };
}

//
// Configuration
//

void Connection::setConnectionHandle(Connection_Handle hdl) {
    connection_hdl_ = hdl;
}

// HERE(ale): in case of multiple connections, be careful with the
// context binding when defining lambdas to avoid races.

void Connection::setOnOpenCallback(Event_Callback callback) {
    onOpen_callback_ = callback;
}

void Connection::setOnCloseCallback(Event_Callback callback) {
    onClose_callback_ = callback;
}

void Connection::setOnFailCallback(Event_Callback callback) {
    onFail_callback_ = callback;
}

void Connection::setOnMessageCallback(OnMessage_Callback callback) {
    onMessage_callback_ = callback;
}

void Connection::setOnPingCallback(Ping_Callback callback) {
    onPing_callback_ = callback;
}

void Connection::setOnPongCallback(Pong_Callback callback) {
    onPong_callback_ = callback;
}

void Connection::setOnPongTimeoutCallback(Pong_Callback callback) {
    onPongTimeout_callback_ = callback;
}

//
// Accessors
//

std::string Connection::getURL() const {
    return url_;
}

Connection_ID Connection::getID() const {
    return id_;
}

Connection_Handle Connection::getConnectionHandle() const {
    return connection_hdl_;
}

// TODO(ale): Need locks? Expensive; better enforce async handlers.
// In case, is it possible to differentiate between read/write locks?
// Use futures?

Connection_State Connection::getState() const {
    return state_;
}

std::string Connection::getErrorReason() const {
    return error_reason_;
}

std::string Connection::getRemoteServer() const {
    return remote_server_;
}

std::string Connection::getRemoteCloseReason() const {
    return remote_close_reason_;
}

Close_Code Connection::getRemoteCloseCode() const {
    return remote_close_code_;
}

//
// Event handlers
//

// TODO(ale): use templates (?)

void Connection::onOpen(Client_Type* client_ptr, Connection_Handle hdl) {
    LOG_TRACE("triggered onOpen");

    state_ = Connection_State_Values::open;
    Client_Type::connection_ptr websocket_ptr { client_ptr->get_con_from_hdl(hdl) };

    std::string server_name { websocket_ptr->get_response_header("Server") };
    if (!server_name.empty()) {
        remote_server_ = server_name;
    }

    onOpen_callback_(client_ptr, shared_from_this());
}

void Connection::onClose(Client_Type* client_ptr, Connection_Handle hdl) {
    LOG_TRACE("triggered onClose");
    state_ = Connection_State_Values::closed;

    Client_Type::connection_ptr websocket_ptr { client_ptr->get_con_from_hdl(hdl) };
    remote_close_reason_ = websocket_ptr->get_remote_close_reason();
    remote_close_code_ = websocket_ptr->get_remote_close_code();

    onClose_callback_(client_ptr, shared_from_this());
}

void Connection::onFail(Client_Type* client_ptr, Connection_Handle hdl) {
    LOG_TRACE("triggered onFail");

    state_ = Connection_State_Values::closed;
    Client_Type::connection_ptr websocket_ptr { client_ptr->get_con_from_hdl(hdl) };

    std::string server_name { websocket_ptr->get_response_header("Server") };
    if (!server_name.empty()) {
        remote_server_ = server_name;
    }

    error_reason_ = websocket_ptr->get_ec().message();

    onFail_callback_(client_ptr, shared_from_this());
}

void Connection::onMessage(Client_Type* client_ptr, Connection_Handle hdl,
                           Client_Type::message_ptr msg) {
    LOG_TRACE("triggered onMessage:\n%1%", msg->get_payload());

    onMessage_callback_(client_ptr, shared_from_this(), msg->get_payload());
}

bool Connection::onPing(Client_Type* client_ptr, Connection_Handle hdl,
                        std::string binary_payload) {
    LOG_TRACE("triggered onPing: %1%", binary_payload);

    return onPing_callback_(client_ptr, shared_from_this(), binary_payload);
}

void Connection::onPong(Client_Type* client_ptr, Connection_Handle hdl,
                        std::string binary_payload) {
    LOG_TRACE("triggered onPong: %1%", binary_payload);

    onPong_callback_(client_ptr, shared_from_this(), binary_payload);
}

void Connection::onPongTimeout(Client_Type* client_ptr, Connection_Handle hdl,
                               std::string binary_payload) {
    LOG_TRACE("triggered onPongTimeout: %1%", binary_payload);

    onPongTimeout_callback_(client_ptr, shared_from_this(), binary_payload);
}

}  // namespace WebSocket
}  // namespace Cthun
