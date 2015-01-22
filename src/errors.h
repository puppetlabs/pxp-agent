#ifndef SRC_ERRORS_H_
#define SRC_ERRORS_H_

#include <stdexcept>
#include <string>

namespace CthunAgent {

/// Base error class.
class agent_error : public std::runtime_error {
  public:
    explicit agent_error(std::string const& msg) : std::runtime_error(msg) {}
};

/// Error due to an invalid command line arguments.
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

/// Generic file error class.
class file_error : public agent_error {
  public:
    explicit file_error(std::string const& msg) : agent_error(msg) {}
};

//
// Data container errors
//

/// Error thrown when a message string is invalid.
class message_parse_error : public agent_error {
  public:
    explicit message_parse_error(std::string const& msg) : agent_error(msg) {}
};

/// Error thrown when a nested message index is invalid.
class message_index_error : public agent_error {
  public:
    explicit message_index_error(std::string const& msg) : agent_error(msg) {}
};

/// Error thrown when trying to set or get invalid type.
class data_type_error : public agent_error {
  public:
    explicit data_type_error(std::string const& msg) : agent_error(msg) {}
};

//
// Configuration container errors
//

class configuration_error : public agent_error {
  public:
    explicit configuration_error(std::string const& msg) : agent_error(msg) {}
};

/// Error thrown when trying to use the configuration object unconfigured
class unconfigured_error : public configuration_error {
  public:
    explicit unconfigured_error(std::string const& msg) : configuration_error(msg) {}
};

/// Error thrown when config file doesn't exist
class no_configfile_error : public configuration_error {
  public:
    explicit no_configfile_error(std::string const& msg) : configuration_error(msg) {}
};

/// Error thrown when required config variable isn't set
class required_not_set_error : public configuration_error {
  public:
    explicit required_not_set_error(std::string const& msg) : configuration_error(msg) {}
};

/// Error thrown when cli parse error occurs
class cli_parse_error : public configuration_error {
  public:
    explicit cli_parse_error(std::string const& msg) : configuration_error(msg) {}
};


}  // namespace CthunAgent

#endif  // SRC_ERRORS_H_
