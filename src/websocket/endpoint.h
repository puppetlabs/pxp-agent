#ifndef SRC_WEBSOCKET_ENDPOINT_H_
#define SRC_WEBSOCKET_ENDPOINT_H_

#include "websocket/connection.h"
#include "websocket/macros.h"

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <thread>

namespace Cthun {
namespace WebSocket {

//
// Endpoint
//

class Endpoint {
  public:
    using Ptr = std::shared_ptr<Endpoint>;

    // TODO(ale): specify root dir for TSL certs instead of each file?
    //            Use a default location? Dot file as Pegasus?
    Endpoint(const std::string& ca_crt_path = "",
             const std::string& client_crt_path = "",
             const std::string& client_key_path = "");
    ~Endpoint();

    /// Open the specified connection.
    /// Throw a connection_error in case of failure.
    void open(Connection::Ptr connection_ptr);

    /// Send message on the specified connection.
    /// Throw a message_error in case of failure.
    void send(Connection::Ptr connection_ptr, std::string msg);

    /// Ping the other endpoint of the specified connection.
    /// Throw a message_error in case of failure.
    void ping(Connection::Ptr connection_ptr,
              const std::string& binary_payload);

    /// Close the specified connection; reason and code are optional
    /// (respectively default to "Closed by client" and 'normal' as
    /// in rfc6455).
    /// Throw a message_error in case of failure.
    void close(Connection::Ptr connection_ptr,
               Close_Code code = Close_Code_Values::normal,
               Message reason = DEFAULT_CLOSE_REASON);

    /// Close all connections that are in 'open' state.
    /// Throw a message_error in case it fails to close any
    /// connection; note that it will attempt to close all of them.
    void closeConnections();

    /// Event loop TLS init callback.
    Context_Ptr onTlsInit(Connection_Handle hdl);

    /// Return the managed connections, regardless of their state.
    std::vector<Connection_ID> getConnectionIDs();

  private:
    std::string ca_crt_path_;
    std::string client_crt_path_;
    std::string client_key_path_;
    Client_Type client_;

    // Keep track of connections to close them all when needed
    std::map<Connection_ID, Connection::Ptr> connections_;
    std::shared_ptr<std::thread> endpoint_thread_;
};

}  // namespace WebSocket
}  // namespace Cthun

#endif  // SRC_WEBSOCKET_ENDPOINT_H_
