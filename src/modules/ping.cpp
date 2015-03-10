#include "src/modules/ping.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.modules.ping"
#include <leatherman/logging/logging.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <valijson/constraints/concrete_constraints.hpp>

#include <ctime>
#include <string>
#include <sstream>

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
    DataContainer data {};

    data.set<std::vector<DataContainer>>(
        request.debug[0].get<std::vector<DataContainer>>("hops"), "request_hops");

    return data;
}

DataContainer Ping::callAction(const std::string& action_name,
                               const ParsedContent& request) {
   return ping(request);
}

}  // namespace Modules
}  // namespace CthunAgent
