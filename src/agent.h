#ifndef SRC_AGENT_H_
#define SRC_AGENT_H_

#include <map>
#include <memory>
#include "module.h"

namespace puppetlabs {
namespace cthun {

class Agent {
public:
    Agent();
    void run();
private:
    std::map<std::string,std::unique_ptr<Module>> modules_;
};


}  // namespace cthun
}  // namespace puppetlabs


#endif  // SRC_AGENT_H_
