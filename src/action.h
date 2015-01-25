#ifndef SRC_AGENT_ACTION_H_
#define SRC_AGENT_ACTION_H_

#include <valijson/schema.hpp>

namespace CthunAgent {

class Action {
  public:
    valijson::Schema input_schema;
    valijson::Schema output_schema;
    std::string behaviour;

    bool isDelayed() const {
        return behaviour.compare("delayed") == 0;
    }
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_H_
