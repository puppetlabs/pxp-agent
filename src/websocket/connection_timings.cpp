#include "src/websocket/connection_timings.h"

#include <sstream>

namespace Cthun {
namespace WebSocket {

//
// ConnectionTimings
//

ConnectionTimings::Duration_Type ConnectionTimings::getTCPInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_Type>(
        tcp_pre_init - start);
}

ConnectionTimings::Duration_Type ConnectionTimings::getHandshakeInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_Type>(
        tcp_post_init - tcp_pre_init);
}

ConnectionTimings::Duration_Type ConnectionTimings::getWebSocketInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_Type>(
        open - start);
}

ConnectionTimings::Duration_Type ConnectionTimings::getCloseInterval() const {
    return std::chrono::duration_cast<ConnectionTimings::Duration_Type>(
        close - start);
}

}  // namespace WebSocket
}  // namespace Cthun
