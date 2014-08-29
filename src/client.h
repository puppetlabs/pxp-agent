#ifndef SRC_CLIENT_H_
#define SRC_CLIENT_H_

#include <cthun-client/src/client.h>

namespace CthunAgent {

class Client : public Cthun::Client::BaseClient {
  public:
    void onMessage_(Cthun::Client::Connection_Handle hdl, Cthun::Client::Client_Configuration::message_ptr msg);

    std::function<void(std::string)> onMessage;
};

}

#endif  // SRC_CLIENT_H_
