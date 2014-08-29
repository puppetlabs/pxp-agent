#include "client.h"
#include "log.h"

namespace CthunAgent {

// this should be part of baseclient
Cthun::Client::Context_Ptr Client::onTlsInit_(websocketpp::connection_hdl hdl) {
    Cthun::Client::Context_Ptr ctx {
        new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1) };

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        std::cout << "### ERROR (tls): " << e.what() << std::endl;
    }
    return ctx;
}

void Client::onMessage_(websocketpp::connection_hdl hdl, Cthun::Client::Client_Configuration::message_ptr msg) {
    onMessage(msg->get_payload());
}

}
