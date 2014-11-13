#include "src/websocket/endpoint.h"
#include "src/websocket/errors.h"
#include "src/common/log.h"
#include "src/common/string_utils.h"

LOG_DECLARE_NAMESPACE("websocket.endpoint");

namespace Cthun {
namespace WebSocket {

Endpoint::Endpoint(const std::string& ca_crt_path,
                   const std::string& client_crt_path,
                   const std::string& client_key_path)
    : ca_crt_path_ { ca_crt_path },
      client_crt_path_ { client_crt_path },
      client_key_path_ { client_key_path } {
    // Turn off websocketpp logging to avoid runtime errors (see CTH-69))
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);

    // Initialize the transport system. Note that in perpetual mode,
    // the event loop does not terminate when there are no connections
    client_.init_asio();
    client_.start_perpetual();

#ifdef CTHUN_CLIENT_SECURE_TRANSPORT
    // Bind the TLS init function
    using websocketpp::lib::bind;
    client_.set_tls_init_handler(bind(&Endpoint::onTlsInit, this, ::_1));
#endif  // CTHUN_CLIENT_SECURE_TRANSPORT

    // Start the event loop thread
    endpoint_thread_.reset(
        new std::thread(&Client_Type::run, &client_));
}

Endpoint::~Endpoint() {
    client_.stop_perpetual();
    closeConnections();
    if (endpoint_thread_ != nullptr && endpoint_thread_->joinable()) {
        endpoint_thread_->join();
    }
}

// open

void Endpoint::open(Connection::Ptr connection_ptr) {
    // Create a websocketpp connection

    websocketpp::lib::error_code ec;
    typename Client_Type::connection_ptr websocket_ptr {
        client_.get_connection(connection_ptr->getURL(), ec) };
    if (ec) {
        throw connection_error {
            "Failed to connect to " + connection_ptr->getURL() + ": "
            + ec.message() };
    }

    // Store the websocket handle in the Connection instance

    Connection_Handle hdl { websocket_ptr->get_handle() };
    connection_ptr->setConnectionHandle(hdl);

    // Bind the event handlers to the connection

    using websocketpp::lib::bind;

    websocket_ptr->set_open_handler(bind(
        &Connection::onOpen, connection_ptr, &client_, ::_1));
    websocket_ptr->set_close_handler(bind(
        &Connection::onClose, connection_ptr, &client_, ::_1));
    websocket_ptr->set_fail_handler(bind(
        &Connection::onFail, connection_ptr, &client_, ::_1));
    websocket_ptr->set_message_handler(bind(
        &Connection::onMessage, connection_ptr, &client_, ::_1, ::_2));
    websocket_ptr->set_ping_handler(bind(
        &Connection::onPing, connection_ptr, &client_, ::_1, ::_2));
    websocket_ptr->set_pong_handler(bind(
        &Connection::onPong, connection_ptr, &client_, ::_1, ::_2));
    websocket_ptr->set_pong_timeout_handler(bind(
        &Connection::onPongTimeout, connection_ptr, &client_, ::_1, ::_2));

    // Finally, open the connection

    client_.connect(websocket_ptr);

    // Keep track of the Connection instance

    connections_.insert(std::pair<Connection_ID, Connection::Ptr> {
        connection_ptr->getID(), connection_ptr });
}

// send

void Endpoint::send(Connection::Ptr connection_ptr, std::string msg) {
    websocketpp::lib::error_code ec;
    client_.send(connection_ptr->getConnectionHandle(), msg,
                 websocketpp::frame::opcode::text, ec);
    if (ec) {
        throw message_error { "Failed to send message: " + ec.message() };
    }
}

// ping (NB: no need for pong(); this is client-oriented)

void Endpoint::ping(Connection::Ptr connection_ptr,
                    const std::string& binary_payload) {
    websocketpp::lib::error_code ec;
    client_.ping(connection_ptr->getConnectionHandle(), binary_payload, ec);
    if (ec) {
        throw message_error { "Failed to ping: " + ec.message() };
    }
}

// close

void Endpoint::close(Connection::Ptr connection_ptr, Close_Code code,
                     Message reason) {
    websocketpp::lib::error_code ec;

    LOG_DEBUG("About to close connection %1%", connection_ptr->getID());

    client_.close(connection_ptr->getConnectionHandle(), code, reason, ec);
    if (ec) {
        throw message_error { "Failed to close connetion "
            + connection_ptr->getID() + ": " + ec.message() };
    }
}

// closeConnections

void Endpoint::closeConnections() {
    // Keep track of failures
    std::vector<Connection_ID> failure_ids;

    for (auto connections_iter = connections_.begin();
         connections_iter != connections_.end(); connections_iter++) {
        if (connections_iter->second->getState()
                == Connection_State_Values::open) {
            try {
                close(connections_iter->second);
            } catch (message_error) {
                failure_ids.push_back(connections_iter->second->getID());
            }
        }
    }

    connections_.clear();

    if (!failure_ids.empty()) {
        std::string message {
            "Failed to close " + std::to_string(failure_ids.size())
            + " connection"
            + Common::StringUtils::plural<Connection_ID>(failure_ids) + ":" };
        for (auto id : failure_ids) {
            message += " " + id;
        }
        throw message_error { message };
    }
}

// TLS init handler (will be bound to all connections)

// NB: for TLS certificates, refer to:
// www.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/ssl__context.html

// TODO(ale): double check boost asio ssl options

Context_Ptr Endpoint::onTlsInit(Connection_Handle hdl) {
    Context_Ptr ctx {
        new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1) };

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
        // TODO(ale): this is what the the debug_client does and it
        // seems to work, nonetheless check if we need to raise here
        LOG_ERROR("failed to configure TLS: %1%", e.what());
    }
    return ctx;
}

// Connection IDs accessor

std::vector<Connection_ID> Endpoint::getConnectionIDs() {
    std::vector<Connection_ID> connection_ids;
    for (auto connection_entry : connections_) {
        connection_ids.push_back(connection_entry.first);
    }
    return connection_ids;
}

}  // namespace WebSocket
}  // namespace Cthun
