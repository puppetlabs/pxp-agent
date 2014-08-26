#ifndef SRC_AGENT_H_
#define SRC_AGENT_H_

#include <map>
#include <memory>
#include "module.h"

namespace CthunAgent {

class Agent {
public:
    Agent();
    void run(std::string module, std::string action);
private:
    std::map<std::string,std::unique_ptr<Module>> modules_;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_H_
