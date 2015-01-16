#ifndef SRC_AGENT_ERRORS_H_
#define SRC_AGENT_ERRORS_H_

#include <stdexcept>
#include <string>

namespace Cthun {
namespace Agent {

/// Base error class.
class agent_error : public std::runtime_error {
  public:
    explicit agent_error(std::string const& msg) : std::runtime_error(msg) {}
};

/// Request error class.
class request_error : public agent_error {
  public:
    explicit request_error(std::string const& msg) : agent_error(msg) {}
};

/// Fatal error class.
class fatal_error : public agent_error {
  public:
    explicit fatal_error(std::string const& msg) : agent_error(msg) {}
};

/// Message validation error class.
class validation_error : public agent_error {
  public:
    explicit validation_error(std::string const& msg) : agent_error(msg) {}
};

/// Module error class.
class module_error : public agent_error {
  public:
    explicit module_error(std::string const& msg) : agent_error(msg) {}
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_ERRORS_H_
