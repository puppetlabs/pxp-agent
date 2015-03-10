#include "src/websocket/endpoint.h"
#include "src/websocket/errors.h"
#include "src/string_utils.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.websocket.endpoint"
#include <leatherman/logging/logging.hpp>

// HERE(ale): we are inheriting openssl include from elsewhere

#include <chrono>
#include <cstdio>

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

namespace CthunAgent {
namespace WebSocket {

static const uint CONNECTION_MIN_INTERVAL { 200000 };  // [us]
static const uint CONNECTION_BACKOFF_LIMIT { 33 };  // [s]
static const uint CONNECTION_BACKOFF_MULTIPLIER { 2 };

Endpoint::Endpoint(const std::string& server_url,
                   const std::string& ca_crt_path,
                   const std::string& client_crt_path,
                   const std::string& client_key_path)
        : server_url_ { server_url },
          ca_crt_path_ { ca_crt_path },
          client_crt_path_ { client_crt_path },
          client_key_path_ { client_key_path },
          connection_state_ { ConnectionStateValues::initialized } {
    // Turn off websocketpp logging to avoid runtime errors (see CTH-69)
    endpoint_.clear_access_channels(websocketpp::log::alevel::all);
    endpoint_.clear_error_channels(websocketpp::log::elevel::all);

    // Initialize the transport system. Note that in perpetual mode,
    // the event loop does not terminate when there are no connections
    endpoint_.init_asio();
    endpoint_.start_perpetual();

    try {
        // Bind the event handlers
        using websocketpp::lib::bind;
        endpoint_.set_tls_init_handler(bind(&Endpoint::onTlsInit, this, ::_1));
        endpoint_.set_open_handler(bind(&Endpoint::onOpen, this, ::_1));
        endpoint_.set_close_handler(bind(&Endpoint::onClose, this, ::_1));
        endpoint_.set_fail_handler(bind(&Endpoint::onFail, this, ::_1));
        endpoint_.set_message_handler(bind(&Endpoint::onMessage, this, ::_1, ::_2));
        endpoint_.set_ping_handler(bind(&Endpoint::onPing, this, ::_1, ::_2));
        endpoint_.set_pong_handler(bind(&Endpoint::onPong, this, ::_1, ::_2));
        endpoint_.set_pong_timeout_handler(bind(&Endpoint::onPongTimeout, this, ::_1, ::_2));
        endpoint_.set_tcp_pre_init_handler(bind(&Endpoint::onPreTCPInit, this, ::_1));
        endpoint_.set_tcp_post_init_handler(bind(&Endpoint::onPostTCPInit, this, ::_1));

        // Start the event loop thread
        endpoint_thread_.reset(new std::thread(&Client_Type::run, &endpoint_));
    } catch (...) {
        LOG_DEBUG("Failed to configure the websocket endpoint; about to stop "
                  "the event loop");
        cleanUp_();
        throw endpoint_error { "failed to initialize" };
    }
}

Endpoint::~Endpoint() {
    cleanUp_();
}

ConnectionState Endpoint::getConnectionState() {
    return connection_state_.load();
}

void Endpoint::setOnOpenCallback(std::function<void()> c_b) {
    on_open_callback_ = c_b;
}

void Endpoint::setOnMessageCallback(std::function<void(std::string msg)> c_b) {
    on_message_callback_ = c_b;
}

void Endpoint::resetCallbacks() {
    on_open_callback_ = [](){};
    on_message_callback_ = [](std::string message){};
}

//
// Synchronous calls
//

void Endpoint::connect(int max_connect_attempts) {
    // FSM: states are ConnectionStateValues; as for the transitions,
    //      we assume that the connection_state_:
    // - can be set to 'initialized' only by the Endpoint constructor;
    // - is set to 'connecting' by connect_();
    // - after a connect_() call, it will become, eventually, open or
    //   closed.

    ConnectionState previous_c_s = connection_state_.load();
    ConnectionState current_c_s;
    int idx { 0 };
    bool try_again { true };
    bool got_max_backoff { false };

    do {
        current_c_s = connection_state_.load();
        idx++;
        if (max_connect_attempts) {
            try_again = (idx < max_connect_attempts);
        }
        got_max_backoff |= (connection_backoff_s_ * CONNECTION_BACKOFF_MULTIPLIER
                            >= CONNECTION_BACKOFF_LIMIT);

        switch (current_c_s) {
        case(ConnectionStateValues::initialized):
            assert(previous_c_s == ConnectionStateValues::initialized);
            connect_();
            usleep(CONNECTION_MIN_INTERVAL);
            break;

        case(ConnectionStateValues::connecting):
            previous_c_s = current_c_s;
            usleep(CONNECTION_MIN_INTERVAL);
            continue;

        case(ConnectionStateValues::open):
            if (previous_c_s != ConnectionStateValues::open) {
                LOG_INFO("Successfully established connection to Cthun server");
                connection_backoff_s_ = CONNECTION_BACKOFF_S;
            }
            return;

        case(ConnectionStateValues::closing):
            previous_c_s = current_c_s;
            usleep(CONNECTION_MIN_INTERVAL);
            continue;

        case(ConnectionStateValues::closed):
            assert(previous_c_s != ConnectionStateValues::open);
            if (previous_c_s == ConnectionStateValues::closed) {
                connect_();
                usleep(CONNECTION_MIN_INTERVAL);
                previous_c_s = ConnectionStateValues::connecting;
            } else {
                LOG_INFO("Failed to connect; retrying in %1% seconds",
                         connection_backoff_s_);
                sleep(connection_backoff_s_);
                connect_();
                usleep(CONNECTION_MIN_INTERVAL);
                if (try_again && !got_max_backoff) {
                    connection_backoff_s_ *= CONNECTION_BACKOFF_MULTIPLIER;
                }
            }
            break;
        }
    } while (try_again);

    connection_backoff_s_ = CONNECTION_BACKOFF_S;
    throw connection_error { "failed to connect after " + std::to_string(idx)
                             + " attempt" + StringUtils::plural(idx) };
}

void Endpoint::send(std::string msg) {
    websocketpp::lib::error_code ec;
    endpoint_.send(connection_handle_, msg, websocketpp::frame::opcode::binary, ec);
    if (ec) {
        throw message_error { "failed to send message: " + ec.message() };
    }
}

void Endpoint::send(void* const serialized_msg_ptr, size_t msg_len) {
    websocketpp::lib::error_code ec;
    endpoint_.send(connection_handle_,
                   serialized_msg_ptr,
                   msg_len,
                   websocketpp::frame::opcode::binary,
                   ec);
    if (ec) {
        throw message_error { "failed to send message: " + ec.message() };
    }
}

void Endpoint::ping(const std::string& binary_payload) {
    websocketpp::lib::error_code ec;
    endpoint_.ping(connection_handle_, binary_payload, ec);
    if (ec) {
        throw message_error { "failed to ping: " + ec.message() };
    }
}

void Endpoint::close(Close_Code code, const std::string& reason) {
    LOG_DEBUG("About to close connection");
    websocketpp::lib::error_code ec;
    endpoint_.close(connection_handle_, code, reason, ec);
    if (ec) {
        throw message_error { "failed to close connetion: " + ec.message() };
    }
}

//
// Private methods
//

void Endpoint::cleanUp_() {
    endpoint_.stop_perpetual();
    if (connection_state_ == ConnectionStateValues::open) {
        close();
    }
    if (endpoint_thread_ != nullptr && endpoint_thread_->joinable()) {
        endpoint_thread_->join();
    }
}

void Endpoint::connect_() {
    connection_state_ = ConnectionStateValues::connecting;
    connection_timings_ = ConnectionTimings();
    websocketpp::lib::error_code ec;
    Client_Type::connection_ptr connection_ptr {
        endpoint_.get_connection(server_url_, ec) };
    if (ec) {
        throw connection_error { "failed to connect to " + server_url_ + ": "
                                 + ec.message() };
    }
    connection_handle_ = connection_ptr->get_handle();
    endpoint_.connect(connection_ptr);
}

//
// Event handlers (private)
//

Context_Ptr Endpoint::onTlsInit(Connection_Handle hdl) {
    LOG_TRACE("WebSocket TLS initialization event");
    // NB: for TLS certificates, refer to:
    // www.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/ssl__context.html
    Context_Ptr ctx { new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1) };
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
        LOG_ERROR("Failed to configure TLS: %1%", e.what());
    }
    return ctx;
}

void Endpoint::onClose(Connection_Handle hdl) {
    connection_timings_.close = std::chrono::high_resolution_clock::now();
    LOG_TRACE("WebSocket connection closed");
    connection_state_ = ConnectionStateValues::closed;
}

void Endpoint::onFail(Connection_Handle hdl) {
    connection_timings_.close = std::chrono::high_resolution_clock::now();
    connection_timings_.connection_failed = true;
    LOG_DEBUG("WebSocket on fail event - %1%", connection_timings_.toString());
    connection_state_ = ConnectionStateValues::closed;
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

void Endpoint::onPreTCPInit(Connection_Handle hdl) {
    connection_timings_.tcp_pre_init = std::chrono::high_resolution_clock::now();
    LOG_TRACE("WebSocket pre-TCP initialization event");
}

void Endpoint::onPostTCPInit(Connection_Handle hdl) {
    connection_timings_.tcp_post_init = std::chrono::high_resolution_clock::now();
    LOG_TRACE("WebSocket post-TCP initialization event");
}

void Endpoint::onOpen(Connection_Handle hdl) {
    connection_timings_.open = std::chrono::high_resolution_clock::now();
    connection_timings_.connection_started = true;
    LOG_DEBUG("WebSocket on open event - %1%", connection_timings_.toString());
    if (on_open_callback_) {
        try {
            on_open_callback_();
            LOG_TRACE("WebSocket connection established");
            connection_state_ = ConnectionStateValues::open;
            return;
        } catch (std::exception&  e) {
            LOG_ERROR("%1%; closing the connection", e.what());
        } catch (...) {
            LOG_ERROR("On open callback failure; closing the connection");
        }
    }
    close(Close_Code_Values::normal, "failed to execute the onOpen callback");
}

void Endpoint::onMessage(Connection_Handle hdl, Client_Type::message_ptr msg) {
    LOG_TRACE("WebSocket onMessage event:\n%1%", msg->get_payload());
    if (on_message_callback_) {
        try {
            // NB: on_message_callback_ should not raise; in case of
            // failure; it must be able to notify back the error...
            on_message_callback_(msg->get_payload());
        } catch (std::exception&  e) {
            LOG_ERROR("Unexpected error during onMessage: %1%", e.what());
        } catch (...) {
            LOG_ERROR("Unexpected error during onMessage");
        }
    }
}

}  // namespace WebSocket
}  // namespace CthunAgent
