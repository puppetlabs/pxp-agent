#ifndef SRC_AGENT_ACTION_OUTCOME_HPP_
#define SRC_AGENT_ACTION_OUTCOME_HPP_

#include <cthun-client/data_container/data_container.hpp>

#include <string>

namespace CthunAgent {

struct ActionOutcome {
    enum class Type { Internal, External };

    Type type;
    std::string stderr;
    std::string stdout;

    // TODO(ale): use leatherman::json_container::JsonContainer
    CthunClient::DataContainer results;

    ActionOutcome() {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  CthunClient::DataContainer& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(std::string& stderr_,
                  std::string& stdout_,
                  CthunClient::DataContainer&& results_)
            : type { Type::External },
              stderr { stderr_ },
              stdout { stdout_ },
              results { results_ } {
    }

    ActionOutcome(CthunClient::DataContainer& results_)
            : type { Type::Internal },
              results { results_ } {
    }

    ActionOutcome(CthunClient::DataContainer&& results_)
            : type { Type::Internal },
              results { results_ } {
    }
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_OUTCOME_HPP_
