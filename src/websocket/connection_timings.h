#ifndef SRC_WEBSOCKET_CONNECTION_TIMINGS_H_
#define SRC_WEBSOCKET_CONNECTION_TIMINGS_H_

#include <chrono>
#include <string>

namespace Cthun {
namespace WebSocket {

//
// ConnectionTimings
//

struct ConnectionTimings {
    using Duration_Type = std::chrono::duration<int, std::micro>;

    std::chrono::high_resolution_clock::time_point start =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point tcp_pre_init;
    std::chrono::high_resolution_clock::time_point tcp_post_init;
    std::chrono::high_resolution_clock::time_point open;
    std::chrono::high_resolution_clock::time_point close;

    bool connection_started {false};
    bool connection_failed {false};

    /// Time interval to establish the TCP connection [us]
    Duration_Type getTCPInterval() const;

    /// Time interval to perform the WebSocket handshake [us]
    Duration_Type getHandshakeInterval() const;

    /// Time interval to establish the WebSocket connection [us]
    Duration_Type getWebSocketInterval() const;

    /// Time interval until close or fail event [us]
    Duration_Type getCloseInterval() const;

    /// Returns a string with the timings
    std::string toString() const;
};

inline std::string ConnectionTimings::toString() const {
    if (connection_started) {
        return "connection timings: TCP "
             + std::to_string(getTCPInterval().count()) + " us, WS handshake "
             + std::to_string(getHandshakeInterval().count())
             + " us, overall " + std::to_string(getWebSocketInterval().count())
             + " us";
    }
    if (connection_failed) {
        return "time to failure "
             + std::to_string(getCloseInterval().count()) + " us";
    }
    return "the endpoint has not been connected yet";
}

}  // namespace WebSocket
}  // namespace Cthun

#endif  // SRC_WEBSOCKET_CONNECTION_TIMINGS_H_
