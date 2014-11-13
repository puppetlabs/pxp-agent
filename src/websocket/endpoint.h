#ifndef SRC_WEBSOCKET_ENDPOINT_H_
#define SRC_WEBSOCKET_ENDPOINT_H_

#include "src/websocket/macros.h"

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace Cthun {
namespace WebSocket {

static const std::string PING_PAYLOAD_DEFAULT { "cthun-agent ping payload" };
static const uint32_t CONNECTION_BACKOFF_S { 2 };  // [us]

//
// Endpoint
//

class Endpoint {
  public:
    Endpoint(const std::string& server_url,
             const std::string& ca_crt_path,
             const std::string& client_crt_path,
             const std::string& client_key_path);

    ~Endpoint();

    Connection_State getConnectionState();

    /// Set the onOpen callback.
    void setOnOpenCallback(std::function<void()> onOpen_callback);

    /// Set the onMessage callback.
    void setOnMessageCallback(
        std::function<void(std::string msg)> onMessage_callback);

    // Reset the onOpen and onMessage callbacks.
    void resetCallbacks();

    //
    // Synchronous calls
    //

    /// Check the state of the connection; in case it's not open,
    /// try to re-open it. This is done for max_connect_attempts
    /// times (or idefinetely, in case, as by default, it's 0), by
    /// following an exponential backoff.
    /// Throw a connection_error if it fails to open.
    void connect(size_t max_connect_attempts = 0);

    /// Send the message to the server.
    /// Throw a message_error in case of failure.
    void send(std::string msg);

    /// Ping the server.
    /// Throw a message_error in case of failure.
    void ping(const std::string& binary_payload = PING_PAYLOAD_DEFAULT);

    /// Close the connection; reason and code are optional
    /// (respectively default to "Closed by client" and 'normal' as
    /// in rfc6455).
    /// Throw a message_error in case of failure.
    void close(Close_Code code = Close_Code_Values::normal,
               const std::string& reason = DEFAULT_CLOSE_REASON);

  private:
    // Cthun server url
    std::string server_url_;

    // TLS certificates
    std::string ca_crt_path_;
    std::string client_crt_path_;
    std::string client_key_path_;

    // Transport layer connection handle
    Connection_Handle connection_handle_;

    // State of the connection (connectig, open, closing, or closed)
    std::atomic<Connection_State> connection_state_;

    // Consecutive pong timeouts counter
    uint consecutive_pong_timeouts_ { 0 };

    // Transport layer endpoint instance
    Client_Type endpoint_;

    // Transport layer event loop thread
    std::shared_ptr<std::thread> endpoint_thread_;

    // Callback function called by the onOpen handler.
    std::function<void()> onOpen_callback_;

    // Callback function called by the onMessage handler.
    std::function<void(std::string message)> onMessage_callback_;

    // Exponential backoff interval for re-connect
    uint32_t connection_backoff_s_ { CONNECTION_BACKOFF_S };

    // Connect the endpoint
    void connect_();

    // Event handlers
    Context_Ptr onTlsInit(Connection_Handle hdl);
    void onClose(Connection_Handle hdl);
    void onFail(Connection_Handle hdl);
    bool onPing(Connection_Handle hdl, std::string binary_payload);
    void onPong(Connection_Handle hdl, std::string binary_payload);
    void onPongTimeout(Connection_Handle hdl, std::string binary_payload);

    /// Handler executed by the transport layer in case of a
    /// WebSocket onOpen event. Calls onOpen_callback_(); in case it
    /// fails, the exception is filtered and the connection is closed.
    void onOpen(Connection_Handle hdl);

    /// Handler executed by the transport layer in case of a
    /// WebSocket onMessage event. Calls onMessage_callback_();
    //  in case it fails, the exception is filtered and logged.
    void onMessage(Connection_Handle hdl, Client_Type::message_ptr msg);
};

}  // namespace WebSocket
}  // namespace Cthun

#endif  // SRC_WEBSOCKET_ENDPOINT_H_
