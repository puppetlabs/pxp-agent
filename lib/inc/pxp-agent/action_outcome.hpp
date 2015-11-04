#ifndef SRC_AGENT_ACTION_OUTCOME_HPP_
#define SRC_AGENT_ACTION_OUTCOME_HPP_

#include <leatherman/json_container/json_container.hpp>

#include <string>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

struct ActionOutcome {
    enum class Type { Internal, External };

    Type type;
    int exitcode;
    std::string std_err;
    std::string std_out;
    lth_jc::JsonContainer results;

    ActionOutcome() {
    }

    ActionOutcome(int exitcode_,
                  std::string& stderr_,
                  std::string& stdout_,
                  lth_jc::JsonContainer& results_)
            : type { Type::External },
              exitcode { exitcode_ },
              std_err { stderr_ },
              std_out { stdout_ },
              results { results_ } {
    }

    ActionOutcome(int exitcode_,
                  std::string&& stderr_,
                  std::string&& stdout_,
                  lth_jc::JsonContainer&& results_)
            : type { Type::External },
              exitcode { exitcode_ },
              std_err { stderr_ },
              std_out { stdout_ },
              results { results_ } {
    }

    ActionOutcome(int exitcode_,
                  lth_jc::JsonContainer& results_)
            : type { Type::Internal },
              exitcode { exitcode_ },
              results { results_ } {
    }

    ActionOutcome(int exitcode_,
                  lth_jc::JsonContainer&& results_)
            : type { Type::Internal },
              exitcode { exitcode_ },
              results { results_ } {
    }
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_OUTCOME_HPP_
