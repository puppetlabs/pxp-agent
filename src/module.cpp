#include "module.h"
#include "log.h"
#include "schemas.h"

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>

namespace CthunAgent {

void Module::call_action(std::string action, const Json::Value& input, Json::Value& output) {
    BOOST_LOG_TRIVIAL(info) << "Invoking native action " << action;
}

void Module::validate_and_call_action(std::string action, const Json::Value& input, Json::Value& output) {
    const Action& action_to_invoke = actions[action];

    BOOST_LOG_TRIVIAL(info) << "validating input for " << name << " " << action;
    std::vector<std::string> errors;
    if (!Schemas::validate(input, action_to_invoke.input_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        throw;
    }

    call_action(action, input, output);

    BOOST_LOG_TRIVIAL(info) << "validating output for " << name << " " << action;
    if (!Schemas::validate(output, action_to_invoke.output_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        throw;
    }

    BOOST_LOG_TRIVIAL(info) << "validated OK: " << output.toStyledString();
}

}
