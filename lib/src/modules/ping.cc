#include <cthun-agent/modules/ping.hpp>
#include <cthun-agent/errors.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <ctime>
#include <string>
#include <sstream>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.modules.ping"
#include <leatherman/logging/logging.hpp>

namespace CthunAgent {
namespace Modules {

namespace V_C = valijson::constraints;

static const std::string PING { "ping" };

Ping::Ping() {
    module_name = PING;

    CthunClient::Schema input_schema { PING };
    input_schema.addConstraint("sender_timestamp",
                               CthunClient::TypeConstraint::String);

    CthunClient::Schema output_schema { PING };

    actions[PING] = Action { "interactive" };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

CthunClient::DataContainer Ping::ping(
                                const CthunClient::ParsedChunks& parsed_chunks) {
    CthunClient::DataContainer data {};

    if (parsed_chunks.debug.empty()) {
        LOG_ERROR("Found no debug entry in the request message");
        throw request_processing_error { "no debug entry" };
    }

    // TODO(ale): revisit this once we formalize the debug format
    try {
        CthunClient::DataContainer debug_entry { parsed_chunks.debug[0] };

        data.set<std::vector<CthunClient::DataContainer>>(
                "request_hops",
                debug_entry.get<std::vector<CthunClient::DataContainer>>("hops"));
    } catch (CthunClient::data_parse_error& e) {
        LOG_ERROR("Failed to parse debug entry: %1%", e.what());
        LOG_DEBUG("Debug entry: %1%", parsed_chunks.debug[0].toString());
        throw request_processing_error { "debug entry is not valid JSON" };
    }
    return data;
}

CthunClient::DataContainer Ping::callAction(
                                const std::string& action_name,
                                const CthunClient::ParsedChunks& parsed_chunks) {
   return ping(parsed_chunks);
}

}  // namespace Modules
}  // namespace CthunAgent
