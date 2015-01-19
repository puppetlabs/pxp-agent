#ifndef SRC_WEBSOCKET_ERRORS_H_
#define SRC_WEBSOCKET_ERRORS_H_

#include <stdexcept>
#include <string>

namespace CthunAgent {
namespace WebSocket {

/// Base error class.
class websocket_error : public std::runtime_error {
  public:
    explicit websocket_error(std::string const& msg) : std::runtime_error(msg) {}
};

/// Endpoint error class.
class endpoint_error : public websocket_error {
  public:
    explicit endpoint_error(std::string const& msg) : websocket_error(msg) {}
};

/// Connection error class.
class connection_error : public websocket_error {
  public:
    explicit connection_error(std::string const& msg) : websocket_error(msg) {}
};

/// Message sending error class.
class message_error : public websocket_error {
  public:
    explicit message_error(std::string const& msg) : websocket_error(msg) {}
};

/// File not found error class.
class file_not_found_error : public websocket_error {
  public:
    explicit file_not_found_error(std::string const& msg) : websocket_error(msg) {}
};

}  // namespace WebSocket
}  // namespace CthunAgent

#endif  // SRC_WEBSOCKET_ERRORS_H_
