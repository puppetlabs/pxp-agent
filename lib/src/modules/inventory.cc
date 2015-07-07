#include <cthun-agent/modules/inventory.hpp>
#include <cthun-agent/errors.hpp>

#include <facter/version.h>
#include <facter/facts/collection.hpp>

#include <sstream>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.modules.inventory"
#include <leatherman/logging/logging.hpp>

namespace CthunAgent {
namespace Modules {

static const std::string INVENTORY { "inventory" };

Inventory::Inventory() {
    module_name = INVENTORY;
    actions.push_back(INVENTORY);
    // Inventory data has only the JSON Object type constraint
    CthunClient::Schema input_schema { INVENTORY };
    CthunClient::Schema output_schema { INVENTORY };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

ActionOutcome Inventory::callAction(const ActionRequest& request) {
    std::ostringstream fact_stream;
    lth_jc::JsonContainer results {};

    facter::facts::collection facts;
#if LIBFACTER_VERSION_MAJOR >= 3
    facts.add_default_facts(false);
#else
    facts.add_default_facts();
#endif
    facts.write(fact_stream, facter::facts::format::json);

    LOG_TRACE("facts: %1%", fact_stream.str());
    results.set<std::string>("facts", fact_stream.str());

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
