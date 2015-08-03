#ifndef SRC_AGENT_CONFIGURATION_HPP_
#define SRC_AGENT_CONFIGURATION_HPP_

#include <string>

namespace CthunAgent {

struct AgentConfiguration {
    std::string modules_dir;
    std::string server_url;
    std::string ca;
    std::string crt;
    std::string key;
    std::string spool_dir;
    std::string modules_config_dir;
    std::string client_type;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_CONFIGURATION_HPP_
