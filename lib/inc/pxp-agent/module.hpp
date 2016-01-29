#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <pxp-agent/action_outcome.hpp>
#include <pxp-agent/action_request.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>      // ParsedChunks
#include <cpp-pcp-client/validator/validator.hpp>  // Validator

#include <leatherman/json_container/json_container.hpp>

#include <vector>
#include <string>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

class Module {
  public:
    enum class Type { Internal, External };

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
    PCPClient::Validator config_validator_;
    PCPClient::Validator input_validator_;
    PCPClient::Validator results_validator_;

    Module();

    /// Whether or not the module has the specified action.
    bool hasAction(const std::string& action_name);

    /// The type of the module.
    virtual Type type() { return Type::Internal; }

    /// Call the specified action.
    /// Return an ActionOutcome instance containing the action outcome.
    /// Throw a Module::ProcessingError in case it fails to execute
    /// the action or if the action returns an invalid output.
    ActionOutcome executeAction(const ActionRequest& request);

  protected:
    virtual ActionOutcome callAction(const ActionRequest& request) = 0;
};

}  // namespace PXPAgent

#endif  // SRC_MODULE_H_
