#ifndef SRC_CLIENT_H_
#define SRC_CLIENT_H_

#include <cthun-client/src/client.h>

namespace CthunAgent {

class Client : public Cthun::Client::BaseClient {
  public:
    void onOpen_(websocketpp::connection_hdl hdl) {};
    void onClose_(websocketpp::connection_hdl hdl) {};
    void onFail_(websocketpp::connection_hdl hdl) {};

    Cthun::Client::Context_Ptr onTlsInit_(websocketpp::connection_hdl hdl);
    void onMessage_(websocketpp::connection_hdl hdl, Cthun::Client::Client_Configuration::message_ptr msg);

    std::function<void(std::string)> onMessage;
};

}

#endif  // SRC_CLIENT_H_
