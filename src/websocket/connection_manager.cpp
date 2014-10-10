#include "websocket/connection_manager.h"
#include "common/log.h"
#include "common/file_utils.h"

LOG_DECLARE_NAMESPACE("websocket.connection_manager");

namespace Cthun {
namespace WebSocket {

ConnectionManager& ConnectionManager::Instance() {
    static ConnectionManager instance;
    return instance;
}

void ConnectionManager::configureSecureEndpoint(
        const std::string& ca_crt_path,
        const std::string& client_crt_path,
        const std::string& client_key_path) {
    if (endpoint_running_) {
        throw endpoint_error { "an endpoint has already started" };
    }

    for (auto crt : std::vector<std::string> {
                        ca_crt_path, client_crt_path, client_key_path }) {
        if (crt.empty()) {
            throw endpoint_error { "not all certificates were specified" };
        }
        if (!Common::FileUtils::fileExists(crt)) {
            throw endpoint_error { crt + " does not exist" };
        }
    }

    ca_crt_path_ = ca_crt_path;
    client_crt_path_ = client_crt_path;
    client_key_path_ = client_key_path;
    is_secure_ = true;
}

void ConnectionManager::resetEndpoint() {
    if (endpoint_running_) {
        endpoint_ptr_->closeConnections();
        endpoint_ptr_.reset();
        endpoint_running_ = false;
    }
}

Connection::Ptr ConnectionManager::createConnection(std::string url) {
    return Connection::Ptr(new Connection(url));
}

void ConnectionManager::open(Connection::Ptr connection_ptr) {
    startEndpointIfNeeded();
    endpoint_ptr_->open(connection_ptr);
}

void ConnectionManager::send(Connection::Ptr connection_ptr, Message message) {
    startEndpointIfNeeded();
    endpoint_ptr_->send(connection_ptr, message);
}

void ConnectionManager::ping(Connection::Ptr connection_ptr,
                             const std::string& binary_payload) {
    startEndpointIfNeeded();
    endpoint_ptr_->ping(connection_ptr, binary_payload);
}

void ConnectionManager::close(Connection::Ptr connection_ptr,
                              Close_Code code, Message reason) {
    startEndpointIfNeeded();
    endpoint_ptr_->close(connection_ptr, code, reason);
}

void ConnectionManager::closeAllConnections() {
    if (endpoint_running_) {
        endpoint_ptr_->closeConnections();
    }
}

std::vector<Connection_ID> ConnectionManager::getConnectionIDs() {
    if (endpoint_running_) {
        return endpoint_ptr_->getConnectionIDs();
    }
    return std::vector<Connection_ID> {};
}

//
// Private interface
//

ConnectionManager::ConnectionManager()
    : endpoint_ptr_ { nullptr },
      endpoint_running_ { false } {
}

void ConnectionManager::startEndpointIfNeeded() {
    if (!endpoint_running_) {
        endpoint_running_ = true;
        if (is_secure_) {
            LOG_INFO("initializing secure endpoint");
            endpoint_ptr_.reset(
                new Endpoint(ca_crt_path_, client_crt_path_, client_key_path_));
        } else {
            LOG_INFO("initializing endpoint");
            endpoint_ptr_.reset(new Endpoint());
        }
    }
}

}  // namespace WebSocket
}  // namespace Cthun
