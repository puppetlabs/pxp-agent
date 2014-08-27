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
    void list_modules();
private:
    std::map<std::string,std::shared_ptr<Module>> modules_;
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_H_
