#include <pxp-agent/modules/ping.hpp>

#include <leatherman/locale/locale.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <string>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.ping"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace lth_jc  = leatherman::json_container;
namespace lth_loc = leatherman::locale;

static const std::string PING { "ping" };

Ping::Ping() {
    module_name = PING;
    actions.push_back(PING);
    lth_jc::Schema input_schema { PING };
    input_schema.addConstraint("sender_timestamp",
                               lth_jc::TypeConstraint::String);
    lth_jc::Schema output_schema { PING };

    input_validator_.registerSchema(input_schema);
    results_validator_.registerSchema(output_schema);
}

lth_jc::JsonContainer Ping::ping(const ActionRequest& request) {
    lth_jc::JsonContainer data {};

    if (request.parsedChunks().debug.empty()) {
        LOG_ERROR("Found no debug entry in the request message");
        throw Module::ProcessingError { lth_loc::translate("no debug entry") };
    }

    auto& debug_entry = request.parsedChunks().debug[0];

    try {
        data.set<std::vector<lth_jc::JsonContainer>>(
                "request_hops",
                debug_entry.get<std::vector<lth_jc::JsonContainer>>("hops"));
    } catch (lth_jc::data_parse_error& e) {
        LOG_ERROR("Failed to parse debug entry: {1}", e.what());
        LOG_DEBUG("Debug entry: {1}", debug_entry.toString());
        throw Module::ProcessingError {
            lth_loc::translate("debug entry is not valid JSON") };
    }
    return data;
}

ActionResponse Ping::callAction(const ActionRequest& request) {
    ActionResponse response { ModuleType::Internal, request };
    response.setValidResultsAndEnd(ping(request));
    return response;
}

}  // namespace Modules
}  // namespace PXPAgent
