#include "client.h"
#include "log.h"

namespace CthunAgent {

void Client::onMessage_(Cthun::Client::Connection_Handle hdl, Cthun::Client::Client_Configuration::message_ptr msg) {
    onMessage(msg->get_payload());
}

}
