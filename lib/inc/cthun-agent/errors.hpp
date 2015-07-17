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

/// Error due to a request message with invalid content.
class request_validation_error : public request_error {
  public:
    explicit request_validation_error(std::string const& msg)
            : request_error(msg) {}
};

/// Error due to a failure during the processing of a request message.
class request_processing_error : public request_error {
  public:
    explicit request_processing_error(std::string const& msg)
            : request_error(msg) {}
};

}  // namespace CthunAgent

#endif  // SRC_ERRORS_H_
