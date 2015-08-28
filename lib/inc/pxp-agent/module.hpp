#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <cthun-agent/action_outcome.hpp>
#include <cthun-agent/action_request.hpp>

#include <cthun-client/protocol/chunks.hpp>      // ParsedChunks
#include <cthun-client/validator/validator.hpp>  // Validator

#include <leatherman/json_container/json_container.hpp>

#include <vector>
#include <string>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;

class Module {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    struct LoadingError : public Error {
        explicit LoadingError(std::string const& msg) : Error(msg) {}
    };

    struct ProcessingError : public Error {
        explicit ProcessingError(std::string const& msg) : Error(msg) {}
    };

    std::string module_name;
    std::vector<std::string> actions;
    CthunClient::Validator config_validator_;
    CthunClient::Validator input_validator_;
    CthunClient::Validator output_validator_;

    Module();

    /// Whether or not the module has the specified action.
    bool hasAction(const std::string& action_name);

    /// Call the specified action.
    /// Return an ActionOutcome instance containing the action outcome.
    /// Throw a Module::ProcessingError in case it fails to execute
    /// the action or if the action returns an invalid output.
    ActionOutcome executeAction(const ActionRequest& request);

  protected:
    virtual ActionOutcome callAction(const ActionRequest& request) = 0;
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
