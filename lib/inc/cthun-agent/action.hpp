#ifndef SRC_AGENT_ACTION_H_
#define SRC_AGENT_ACTION_H_

#include <cthun-client/validator/schema.hpp>

namespace CthunAgent {

class Action {
  public:
    std::string behaviour;

    bool isDelayed() const {
        return behaviour == "delayed";
    }
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_H_
