#include "src/websocket/endpoint.h"
#include "src/websocket/errors.h"
#include "src/common/log.h"
#include "src/common/string_utils.h"

LOG_DECLARE_NAMESPACE("websocket.endpoint");

namespace Cthun {
namespace WebSocket {

Endpoint::Endpoint(const std::string& server_url,
                   const std::string& ca_crt_path,
                   const std::string& client_crt_path,
                   const std::string& client_key_path)
        : server_url_ { server_url },
          ca_crt_path_ { ca_crt_path },
          client_crt_path_ { client_crt_path },
          client_key_path_ { client_key_path },
          connection_state_ { Connection_State_Values::connecting } {
    // Turn off websocketpp logging to avoid runtime errors (see CTH-69))
    endpoint_.clear_access_channels(websocketpp::log::alevel::all);
    endpoint_.clear_error_channels(websocketpp::log::elevel::all);

    // Initialize the transport system. Note that in perpetual mode,
    // the event loop does not terminate when there are no connections
    endpoint_.init_asio();
    endpoint_.start_perpetual();

    // Bind the event handlers
    using websocketpp::lib::bind;
    endpoint_.set_tls_init_handler(bind(&Endpoint::onTlsInit, this, ::_1));
    endpoint_.set_open_handler(bind(&Endpoint::onOpen, this, ::_1));
    endpoint_.set_close_handler(bind(&Endpoint::onClose, this, ::_1));
    endpoint_.set_fail_handler(bind(&Endpoint::onFail, this, ::_1));
    endpoint_.set_message_handler(bind(&Endpoint::onMessage, this, ::_1, ::_2));
    endpoint_.set_ping_handler(bind(&Endpoint::onPing, this, ::_1, ::_2));
    endpoint_.set_pong_handler(bind(&Endpoint::onPong, this, ::_1, ::_2));
    endpoint_.set_pong_timeout_handler(bind(&Endpoint::onPongTimeout, this,
                                            ::_1, ::_2));

    // Start the event loop thread
    endpoint_thread_.reset(new std::thread(&Client_Type::run, &endpoint_));
}

Endpoint::~Endpoint() {
    endpoint_.stop_perpetual();
    if (connection_state_ == Connection_State_Values::open) {
        close();
    }
    if (endpoint_thread_ != nullptr && endpoint_thread_->joinable()) {
        endpoint_thread_->join();
    }
}

Connection_State Endpoint::getConnectionState() {
    return connection_state_.load();
}

void Endpoint::setOnOpenCallback(std::function<void()> c_b) {
    onOpen_callback_ = c_b;
}

void Endpoint::setOnMessageCallback(std::function<void(std::string msg)> c_b) {
    onMessage_callback_ = c_b;
}

//
// Synchronous calls
//

void Endpoint::connect() {
    // TODO: connection retries
    websocketpp::lib::error_code ec;
    Client_Type::connection_ptr connection_ptr {
        endpoint_.get_connection(server_url_, ec) };
    if (ec) {
        throw connection_error { "Failed to connect to " + server_url_ + ": "
                                 + ec.message() };
    }
    connection_handle_ = connection_ptr->get_handle();
    endpoint_.connect(connection_ptr);
}

void Endpoint::send(std::string msg) {
    websocketpp::lib::error_code ec;
    endpoint_.send(connection_handle_, msg, websocketpp::frame::opcode::text, ec);
    if (ec) {
        throw message_error { "Failed to send message: " + ec.message() };
    }
}

void Endpoint::ping(const std::string& binary_payload) {
    websocketpp::lib::error_code ec;
    endpoint_.ping(connection_handle_, binary_payload, ec);
    if (ec) {
        throw message_error { "Failed to ping: " + ec.message() };
    }
}

void Endpoint::close(Close_Code code, const std::string& reason) {
    LOG_DEBUG("About to close connection");
    websocketpp::lib::error_code ec;
    endpoint_.close(connection_handle_, code, reason, ec);
    if (ec) {
        throw message_error { "Failed to close connetion: " + ec.message() };
    }
}

//
// Event handlers
//

Context_Ptr Endpoint::onTlsInit(Connection_Handle hdl) {
    LOG_TRACE("WebSocket TLS initialization event");
    // NB: for TLS certificates, refer to:
    // www.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/ssl__context.html
    Context_Ptr ctx { new boost::asio::ssl::context(
        boost::asio::ssl::context::tlsv1) };

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::single_dh_use);
        ctx->use_certificate_file(client_crt_path_,
                                  boost::asio::ssl::context::file_format::pem);
        ctx->use_private_key_file(client_key_path_,
                                  boost::asio::ssl::context::file_format::pem);
        ctx->load_verify_file(ca_crt_path_);
    } catch (std::exception& e) {
        LOG_ERROR("failed to configure TLS: %1%", e.what());
    }
    return ctx;
}

void Endpoint::onOpen(Connection_Handle hdl) {
    LOG_TRACE("WebSocket connection established");
    connection_state_ = Connection_State_Values::open;
    onOpen_callback_();
}

void Endpoint::onClose(Connection_Handle hdl) {
    LOG_TRACE("WebSocket connection closed");
    connection_state_ = Connection_State_Values::closed;
}

void Endpoint::onFail(Connection_Handle hdl) {
    LOG_TRACE("WebSocket onFail event");
    connection_state_ = Connection_State_Values::closed;
}

void Endpoint::onMessage(Connection_Handle hdl, Client_Type::message_ptr msg) {
    LOG_TRACE("WebSocket onMessage event:\n%1%", msg->get_payload());
    if (onMessage_callback_) {
        onMessage_callback_(msg->get_payload());
    }
}

bool Endpoint::onPing(Connection_Handle hdl, std::string binary_payload) {
    LOG_TRACE("WebSocket onPing event - payload: %1%", binary_payload);
    // Returning true so the transport layer will send back a pong
    return true;
}

void Endpoint::onPong(Connection_Handle hdl, std::string binary_payload) {
    LOG_DEBUG("WebSocket onPong event");
    if (consecutive_pong_timeouts_) {
        consecutive_pong_timeouts_ = 0;
    }
}

void Endpoint::onPongTimeout(Connection_Handle hdl, std::string binary_payload) {
    LOG_WARNING("WebSocket onPongTimeout event (%1% consecutive)",
                consecutive_pong_timeouts_++);
}

// resetCallbacks
void Endpoint::resetCallbacks() {
    onOpen_callback_ = [](){};
    onMessage_callback_ = [](std::string message){};
}

}  // namespace WebSocket
}  // namespace Cthun
