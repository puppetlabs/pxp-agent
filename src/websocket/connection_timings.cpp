#include "src/websocket/connection_timings.h"

#include <sstream>

namespace Cthun {
namespace WebSocket {

ConnectionTimings::ConnectionTimings()
        : start { std::chrono::high_resolution_clock::now() } {
}

ConnectionTimings::Duration_us ConnectionTimings::getTCPInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_us>(
        tcp_pre_init - start);
}

ConnectionTimings::Duration_us ConnectionTimings::getHandshakeInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_us>(
        tcp_post_init - tcp_pre_init);
}

ConnectionTimings::Duration_us ConnectionTimings::getWebSocketInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_us>(
        open - start);
}

ConnectionTimings::Duration_us ConnectionTimings::getCloseInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_us>(
        close - start);
}

}  // namespace WebSocket
}  // namespace Cthun
