#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <cthun-agent/action_outcome.hpp>
#include <cthun-agent/action_request.hpp>

#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/protocol/chunks.hpp>      // ParsedChunks
#include <cthun-client/validator/validator.hpp>  // Validator

#include <vector>
#include <string>

namespace CthunAgent {

class Module {
  public:
    std::string module_name;
    std::vector<std::string> actions;
    CthunClient::Validator input_validator_;
    CthunClient::Validator output_validator_;

    Module();

    /// Whether or not the module has the specified action.
    bool hasAction(const std::string& action_name);

    /// Call the specified action.
    /// Return an ActionOutcome instance containing the action outcome.
    /// Throw a request_validation_error in case of invalid input.
    /// Throw a request_processing_error in case it fails to execute
    /// the action or if the action returns an invalid output.
    ActionOutcome executeAction(const ActionRequest& request);

  protected:
    virtual ActionOutcome callAction(const ActionRequest& request) = 0;
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
