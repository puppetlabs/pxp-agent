#ifndef SRC_CLIENT_H_
#define SRC_CLIENT_H_

#include <cthun-client/src/client.h>

namespace CthunAgent {

class Client : public Cthun::Client::BaseClient {
  public:
    void onMessage_(Cthun::Client::Connection_Handle hdl, Cthun::Client::Client_Configuration::message_ptr msg) {
        onMessage(msg->get_payload());
    }

    void onOpen_(Cthun::Client::Connection_Handle hdl) {
        onOpen(hdl);
    };

    std::function<void(std::string)> onMessage;
    std::function<void(Cthun::Client::Connection_Handle)> onOpen;
};

}

#endif  // SRC_CLIENT_H_
