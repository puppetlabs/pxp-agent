#ifndef SRC_WEBSOCKET_MACROS_H_
#define SRC_WEBSOCKET_MACROS_H_

// We need this hack to avoid the compilation error described in
// https://github.com/zaphoyd/websocketpp/issues/365
#define _WEBSOCKETPP_NULLPTR_TOKEN_ 0

// See http://www.zaphoyd.com/websocketpp/manual/reference/cpp11-support
// for prepocessor directives and choosing between boost and cpp11
// C++11 STL tokens
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_MEMORY_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_THREAD_

//
// includes
//

#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/client.hpp>
// NB: we must include asio_client.hpp even if CTHUN_CLIENT_SECURE_TRANSPORT
// is not defined in order to define Context_Ptr
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <string>

//
// Tokens and aliases
//

namespace Cthun {
namespace WebSocket {

static const std::string DEFAULT_CLOSE_REASON { "Closed by client" };

// Define the transport layer configuration for the client endpoint;
// used by Endpoint and Connection classes.
using Client_Type = websocketpp::client<websocketpp::config::asio_tls_client>;
using Context_Ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;
using Connection_Handle = websocketpp::connection_hdl;

using Close_Code = websocketpp::close::status::value;
namespace Close_Code_Values = websocketpp::close::status;

using Connection_State = websocketpp::session::state::value;
namespace Connection_State_Values = websocketpp::session::state;

using Frame_Opcode = websocketpp::frame::opcode::value;
namespace Frame_Opcode_Values = websocketpp::frame::opcode;

}  // namespace WebSocket
}  // namespace Cthun

#endif  // SRC_WEBSOCKET_MACROS_H_
