#include "src/modules/ping.h"
#include "src/log.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <valijson/constraints/concrete_constraints.hpp>

#include <ctime>
#include <string>
#include <sstream>

LOG_DECLARE_NAMESPACE("modules.ping");

namespace CthunAgent {
namespace Modules {

namespace V_C = valijson::constraints;

Ping::Ping() {
    module_name = "ping";

    V_C::TypeConstraint json_type_object { V_C::TypeConstraint::kObject };
    V_C::TypeConstraint json_type_string { V_C::TypeConstraint::kString };

    valijson::Schema input_schema;
    V_C::PropertiesConstraint::PropertySchemaMap properties;
    V_C::PropertiesConstraint::PropertySchemaMap pattern_properties;

    properties["sender_timestamp"].addConstraint(json_type_string);
    input_schema.addConstraint(new V_C::PropertiesConstraint(properties,
                                                             pattern_properties));

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_object);

    actions["ping"] = Action { input_schema, output_schema, "interactive" };
}

DataContainer Ping::ping(const ParsedContent& request) {
    int sender_timestamp;
    auto request_input = request.data.get<DataContainer>("params");
    std::istringstream(request_input.get<std::string>("sender_timestamp")) >>
        sender_timestamp;

    boost::posix_time::ptime current_date_microseconds =
        boost::posix_time::microsec_clock::local_time();
    auto current_date_milliseconds =
        current_date_microseconds.time_of_day().total_milliseconds();
    auto time_to_agent = current_date_milliseconds - sender_timestamp;

    DataContainer data {};
    data.set<std::string>(request_input.get<std::string>("sender_timestamp"),
                          "sender_timestamp");
    data.set<std::string>(std::to_string(time_to_agent), "time_to_agent");
    data.set<std::string>(std::to_string(current_date_milliseconds),
                          "agent_timestamp");

    // TODO(ale): copy hops from the debug data
    // if (request.debug.empty()) {
    //     LOG_WARNING("Received no ping timings from server");
    // }

    // for (const auto& debug_entry : request_debug) {

    // }
    // data.set<std::vector<DataContainer>>(
    //     request.debug.get<std::vector<DataContainer>>("hops"), "request_hops");

    return data;
}

DataContainer Ping::callAction(const std::string& action_name,
                               const ParsedContent& request) {
   return ping(request);
}

}  // namespace Modules
}  // namespace CthunAgent
