#ifndef SRC_AGENT_ACTION_OUTCOME_HPP_
#define SRC_AGENT_ACTION_OUTCOME_HPP_

#include <leatherman/json_container/json_container.hpp>

#include <string>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

struct ActionOutcome {
    enum class Type { Internal, External };

    Type type;
    std::string stderr;
    std::string stdout;

    lth_jc::JsonContainer results;

    ActionOutcome() {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  lth_jc::JsonContainer& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  lth_jc::JsonContainer&& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(lth_jc::JsonContainer& results_)
            : type { Type::Internal },
              results { results_ } {
    }

    ActionOutcome(lth_jc::JsonContainer&& results_)
            : type { Type::Internal },
              results { results_ } {
    }
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_OUTCOME_HPP_
