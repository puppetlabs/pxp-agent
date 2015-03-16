#include "src/module.h"
#include "src/errors.h"
#include "src/log.h"

#include <iostream>

LOG_DECLARE_NAMESPACE("module");

namespace CthunAgent {

Module::Module()
        : input_validator_ {},
          output_validator_ {} {
}

CthunClient::DataContainer Module::performRequest(
                        const std::string& action_name,
                        const CthunClient::ParsedChunks& parsed_chunks) {
    // Assuming the request message contains JSON data
    assert(parsed_chunks.has_data);
    assert(parsed_chunks.data_type == CthunClient::ContentType::Json);

    if (actions.find(action_name) == actions.end()) {
        throw request_validation_error { "unknown action '" + action_name
                                         + "' for module " + module_name };
    }

    // Validate request input
    auto action = actions[action_name];
    auto request_input =
        parsed_chunks.data.get<CthunClient::DataContainer>("params");

    try {
        LOG_DEBUG("Validating input for '%1% %2%'", module_name, action_name);
        // NB: the registred schemas have the same name as the action
        input_validator_.validate(request_input, action_name);
    } catch (CthunClient::validation_error) {
        throw request_validation_error { "invalid input for '" + module_name
                                         + " " + action_name + "'" };
    }

    // Execute action
    CthunClient::DataContainer response_output;

    try {
        response_output = callAction(action_name, parsed_chunks);
    } catch (request_processing_error) {
        throw;
    } catch (std::exception& e) {
        LOG_ERROR("Faled to execute '%1% %2%': %3%",
                  module_name, action_name, e.what());
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    } catch (...) {
        LOG_ERROR("Failed to execute '%1% %2%' - unexpected exception",
                  module_name, action_name);
        throw request_processing_error { "failed to execute '" + module_name
                                         + " " + action_name + "'" };
    }

    if (!action.isDelayed()) {
        // Validate the result output
        try {
            LOG_DEBUG("Validating output for '%1% %2%'",
                      module_name, action_name);
            output_validator_.validate(response_output, action_name);
        } catch (CthunClient::validation_error) {
            throw request_processing_error { "the action '" + module_name + " "
                                             + action_name + "' returned an "
                                             "invalid result" };
        }
    }

    return response_output;
}

}  // namespace CthunAgent
