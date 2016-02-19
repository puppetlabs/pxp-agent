#ifndef SRC_AGENT_MODULE_HPP_
#define SRC_AGENT_MODULE_HPP_

#include <pxp-agent/action_response.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/module_type.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>      // ParsedChunks
#include <cpp-pcp-client/validator/validator.hpp>  // Validator

#include <leatherman/json_container/json_container.hpp>

#include <vector>
#include <string>

namespace PXPAgent {

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
    PCPClient::Validator config_validator_;
    PCPClient::Validator input_validator_;
    PCPClient::Validator results_validator_;

    Module();

    /// Whether or not the module has the specified action.
    bool hasAction(const std::string& action_name);

    /// The type of the module.
    virtual ModuleType type() { return ModuleType::Internal; }

    /// Validate the output contained in the ActionResponse instance,
    /// by using the module's 'output' JSON schema. In case of errors,
    /// the response's metadata will be updated ('results_are_valid'
    /// will be set to false, 'execution_error' will be populated, and
    /// 'status' to Failure, but 'end' will not be updated).
    /// This member function does not throw validation errors.
    void validateOutputAndUpdateMetadata(ActionResponse& response);

    /// Call the specified action and validate its results.
    /// Return an ActionResponse instance containing the action
    /// output and the validation outcome.
    /// This function does not throw any error; possible errors
    /// occurred during the action execution or the output validation
    /// will be reported within the ActionOutput instance.
    ActionResponse executeAction(const ActionRequest& request);

  protected:
    /// Subclass implementations should throw a ProcessingError in
    /// case it fails to execute the action.
    virtual ActionResponse callAction(const ActionRequest& request) = 0;
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_MODULE_HPP_
