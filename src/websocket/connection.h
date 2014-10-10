#ifndef SRC_WEBSOCKET_CONNECTION_H_
#define SRC_WEBSOCKET_CONNECTION_H_

#include "src/websocket/macros.h"

#include <websocketpp/common/connection_hdl.hpp>

#include <memory>
#include <string>

namespace Cthun {
namespace WebSocket {

// NB: enable_shared_from_this<> is to create a shared_ptr to *this

class Connection : public std::enable_shared_from_this<Connection> {
  public:
    /// Connection shared pointer type
    using Ptr = std::shared_ptr<Connection>;

    /// Generic event callback type
    using Event_Callback = std::function<void(Client_Type* client_ptr,
                                              Connection::Ptr connection_ptr)>;

    /// OnMessage event callback type
    using OnMessage_Callback = std::function<void(Client_Type* client_ptr,
                                                  Connection::Ptr connection_ptr,
                                                  std::string message)>;

    /// ping callback type
    using Ping_Callback = std::function<bool(Client_Type* client_ptr,
                                             Connection::Ptr connection_ptr,
                                             std::string binary_payload)>;

    /// pong and pongTimeout callback type
    using Pong_Callback = std::function<void(Client_Type* client_ptr,
                                             Connection::Ptr connection_ptr,
                                             std::string binary_payload)>;

    explicit Connection(const std::string& url);

    void waitForOpen(int max_num_checks = 5, int interval_check = 1);

    //
    // Configuration
    //

    /// Connection handle setter.
    void setConnectionHandle(Connection_Handle hdl);

    /// Callback called by onOpen.
    void setOnOpenCallback(Event_Callback callback);

    /// Callback called by onClose.
    void setOnCloseCallback(Event_Callback callback);

    /// Callback called by onFail.
    void setOnFailCallback(Event_Callback callback);

    /// Callback called by onMessage.
    void setOnMessageCallback(OnMessage_Callback callback);

    /// Callback called by onPing.
    void setOnPingCallback(Ping_Callback callback);

    /// Callback called by onPong.
    /// Set it to a function returning false to disable pong responses.
    void setOnPongCallback(Pong_Callback callback);

    /// Callback called by onPongTimeout.
    void setOnPongTimeoutCallback(Pong_Callback callback);

    //
    // Accessors
    //

    /// Returns the remote url
    std::string getURL() const;

    /// Returns the connection ID
    Connection_ID getID() const;

    /// Returns the connection handle used by the underlying transport
    Connection_Handle getConnectionHandle() const;

    /// Returns the current connection state
    Connection_State getState() const;

    /// Returns the error string
    std::string getErrorReason() const;

    /// Returns the remote server, parsed from a received message
    std::string getRemoteServer() const;

    /// Returns the remote close reason
    std::string getRemoteCloseReason() const;

    /// Returns the remote close code
    Close_Code getRemoteCloseCode() const;

    //
    // Event handlers
    //

    // NB: the TLS initialization handler is bound to the Endpoint, so
    // it's applied to all connections. Binding it here doesn't work.

    /// Event handler: on open.
    virtual void onOpen(Client_Type* client_ptr, Connection_Handle hdl);

    /// Event handler: on close.
    virtual void onClose(Client_Type* client_ptr, Connection_Handle hdl);

    /// Event handler: on fail.
    virtual void onFail(Client_Type* client_ptr, Connection_Handle hdl);

    /// Event handler: on message.
    virtual void onMessage(Client_Type* client_ptr, Connection_Handle hdl,
                           Client_Type::message_ptr msg);

    /// Event handler: on message.
    virtual bool onPing(Client_Type* client_ptr, Connection_Handle hdl,
                        std::string binary_payload);

    /// Event handler: on message.
    virtual void onPong(Client_Type* client_ptr, Connection_Handle hdl,
                        std::string binary_payload);

    /// Event handler: on message.
    virtual void onPongTimeout(Client_Type* client_ptr, Connection_Handle hdl,
                               std::string binary_payload);

  protected:
    std::string url_;
    Connection_ID id_;
    Connection_Handle connection_hdl_;
    Connection_State state_;
    std::string error_reason_;
    std::string remote_server_;
    std::string remote_close_reason_;
    Close_Code remote_close_code_;

    Event_Callback onOpen_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr) {} };
    Event_Callback onClose_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr) {} };
    Event_Callback onFail_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr) {} };
    OnMessage_Callback onMessage_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr,
           std::string message) {} };

    // Returning true by default to send back pong
    Ping_Callback onPing_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr,
           std::string binary_payload) { return true; } };

    Pong_Callback onPong_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr,
           std::string binary_payload) {} };
    Pong_Callback onPongTimeout_callback_ {
        [](Client_Type* client_ptr, Connection::Ptr connection_ptr,
           std::string binary_payload) {} };
};

}  // namespace WebSocket
}  // namespace Cthun

#endif  // SRC_WEBSOCKET_CONNECTION_H_
