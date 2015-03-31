#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <cthun-agent/action.hpp>

#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/message/chunks.hpp>       // ParsedChunks
#include <cthun-client/validator/validator.hpp>  // Validator

#include <map>
#include <string>

namespace CthunAgent {

class Module {
  public:
    // TODO(ale): consider rewriting Module, ExternalModule, Action to
    // avoid exposing members
    std::string module_name;
    std::map<std::string, Action> actions;
    CthunClient::Validator input_validator_;
    CthunClient::Validator output_validator_;

    Module();

    /// Performs the requested action.
    virtual CthunClient::DataContainer callAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks) = 0;

    /// Validate the input of the request.
    /// Perform the requested action.
    /// Validate the action output in case of a blocking action.
    /// Return the output of the action as a DataContainer object; the
    /// output will be the result in case of a blocking action, or it
    /// will contain the job id in case of a delayed action.
    /// It is assumed that parsed_chunks contains JSON data.
    /// Throw a request_validation_error in case of unknown action or
    /// invalid request input.
    /// Throw a request_processing_error in case the requested action
    /// fails to execute or if it provides an invalid output.
    CthunClient::DataContainer performRequest(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
