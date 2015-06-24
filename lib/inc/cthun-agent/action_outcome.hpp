#ifndef SRC_AGENT_ACTION_OUTCOME_HPP_
#define SRC_AGENT_ACTION_OUTCOME_HPP_

#include <leatherman/json_container/json_container.hpp>

#include <string>

namespace CthunAgent {

namespace LTH_JC = leatherman::json_container;

struct ActionOutcome {
    enum class Type { Internal, External };

    Type type;
    std::string stderr;
    std::string stdout;

    // TODO(ale): use leatherman::json_container::JsonContainer
    LTH_JC::JsonContainer results;

    ActionOutcome() {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  LTH_JC::JsonContainer& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  LTH_JC::JsonContainer&& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(LTH_JC::JsonContainer& results_)
            : type { Type::Internal },
              results { results_ } {
    }

    ActionOutcome(LTH_JC::JsonContainer&& results_)
            : type { Type::Internal },
              results { results_ } {
    }
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_OUTCOME_HPP_
