#ifndef SRC_AGENT_H_
#define SRC_AGENT_H_

#include "module.h"

#include <cthun-client/src/connection_manager.h>

#include <map>
#include <memory>
#include <string>

namespace CthunAgent {

// TODO(ale): move to a configuration namespace; get values from command line
static std::string DEFAULT_CA { "./test-resources/ssl/ca/ca_crt.pem" };
static std::string DEFAULT_CERT { "./test-resources/ssl/certs/cthun-client.pem" };
static std::string DEFAULT_KEY { "./test-resources/ssl/private_keys/cthun-client.pem" };

class Agent {
  public:
    Agent();
    ~Agent();
    void run(std::string module, std::string action);
    // daemon entry point
    void connect_and_run(std::string url,
                         std::string ca_crt_path = DEFAULT_CA,
                         std::string client_crt_path = DEFAULT_CERT,
                         std::string client_key_path = DEFAULT_KEY);

  private:
    void list_modules();
    void send_login(Cthun::Client::Client_Type* client_ptr);
    void handle_message(Cthun::Client::Client_Type* client_ptr,
                        std::string message);

    std::map<std::string,std::shared_ptr<Module>> modules_;
    Cthun::Client::Connection::Ptr connection_ptr_ { nullptr };
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_H_
