#ifndef SRC_ERRORS_H_
#define SRC_ERRORS_H_

#include <stdexcept>
#include <string>

namespace CthunAgent {

// TODO(ale): consider moving this to the relevant files

/// Base error class.
class agent_error : public std::runtime_error {
  public:
    explicit agent_error(std::string const& msg) : std::runtime_error(msg) {}
};

//
// RPC request errors
//

class request_error : public agent_error {
  public:
    explicit request_error(std::string const& msg) : agent_error(msg) {}
};

}  // namespace CthunAgent

#endif  // SRC_ERRORS_H_
