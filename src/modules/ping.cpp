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

Ping::Ping() {
    module_name = "ping";

    valijson::constraints::TypeConstraint json_type_object {
        valijson::constraints::TypeConstraint::kObject };

    valijson::constraints::TypeConstraint json_type_string {
        valijson::constraints::TypeConstraint::kString };

    valijson::Schema input_schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap pattern_properties;

    properties["sender_timestamp"].addConstraint(json_type_string);
    input_schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                              properties,
                              pattern_properties));

    valijson::Schema output_schema;
    output_schema.addConstraint(json_type_object);

    actions["ping"] = Action { input_schema, output_schema, "interactive" };
}

DataContainer Ping::ping(const DataContainer& request) {
    int sender_timestamp;
    DataContainer input { request.get<DataContainer>("data", "params") };
    std::istringstream(input.get<std::string>("sender_timestamp")) >>
        sender_timestamp;

    boost::posix_time::ptime current_date_microseconds =
        boost::posix_time::microsec_clock::local_time();
    auto current_date_milliseconds =
        current_date_microseconds.time_of_day().total_milliseconds();
    auto time_to_agent = current_date_milliseconds - sender_timestamp;

    DataContainer data {};
    data.set<std::string>(input.get<std::string>("sender_timestamp"),
        "sender_timestamp");
    data.set<std::string>(std::to_string(time_to_agent), "time_to_agent");
    data.set<std::string>(std::to_string(current_date_milliseconds), "agent_timestamp");
    std::vector<DataContainer> hops { request.get<std::vector<DataContainer>>("hops") };
    data.set<std::vector<DataContainer>>(hops, "request_hops");

    return data;
}

DataContainer Ping::callAction(const std::string& action_name,
                               const DataContainer& request) {
   return ping(request);
}

}  // namespace Modules
}  // namespace CthunAgent
