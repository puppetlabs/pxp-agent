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

/// Fatal error class.
class fatal_error : public agent_error {
  public:
    explicit fatal_error(std::string const& msg) : agent_error(msg) {}
};

/// Error thrown when loading modules.
class module_error : public agent_error {
  public:
    explicit module_error(std::string const& msg) : agent_error(msg) {}
};

//
// RPC request errors
//

class request_error : public agent_error {
  public:
    explicit request_error(std::string const& msg) : agent_error(msg) {}
};

/// Error due to a request message with invalid format such that it is
/// not possible to retrieve the content of the data chunk.
class request_format_error : public request_error {
  public:
    explicit request_format_error(std::string const& msg)
            : request_error(msg) {}
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

//
// Configuration container errors
//

class configuration_error : public agent_error {
  public:
    explicit configuration_error(std::string const& msg)
            : agent_error(msg) {}
};

/// Error thrown when trying to set the configuration object before
/// initializing it.
class uninitialized_error : public configuration_error {
  public:
    explicit uninitialized_error(std::string const& msg)
            : configuration_error(msg) {}
};

/// Error thrown when referring to an unknown configuration entry or
/// trying to set an invalid value to a known entry.
class configuration_entry_error : public configuration_error {
  public:
    explicit configuration_entry_error(std::string const& msg)
            : configuration_error(msg) {}
};

/// Error thrown when config file doesn't exist.
class no_configfile_error : public configuration_error {
  public:
    explicit no_configfile_error(std::string const& msg)
            : configuration_error(msg) {}
};

/// Error thrown when required config variable isn't set.
class required_not_set_error : public configuration_error {
  public:
    explicit required_not_set_error(std::string const& msg)
        : configuration_error(msg) {}
};

/// Error thrown when cli parse error occurs.
class cli_parse_error : public configuration_error {
  public:
    explicit cli_parse_error(std::string const& msg)
            : configuration_error(msg) {}
};


}  // namespace CthunAgent

#endif  // SRC_ERRORS_H_
