#include <cthun-agent/modules/inventory.hpp>
#include <cthun-agent/errors.hpp>

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
    CthunClient::DataContainer results {};

    facter::facts::collection facts;
    facts.add_default_facts(false);
    facts.write(fact_stream, facter::facts::format::json);

    LOG_TRACE("facts: %1%", fact_stream.str());
    results.set<std::string>("facts", fact_stream.str());

    return ActionOutcome { results };
}

}  // namespace Modules
}  // namespace CthunAgent
