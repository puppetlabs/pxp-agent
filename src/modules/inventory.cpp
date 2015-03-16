#include "src/modules/inventory.h"
#include "src/log.h"
#include "src/errors.h"

#include <facter/facts/collection.hpp>

#include <sstream>

LOG_DECLARE_NAMESPACE("modules.inventory");

namespace CthunAgent {
namespace Modules {

static const std::string INVENTORY { "inventory" };

Inventory::Inventory() {
    module_name = INVENTORY;

    // Inventory data has only the JSON Object type constraint
    CthunClient::Schema input_schema { INVENTORY };
    CthunClient::Schema output_schema { INVENTORY };

    actions[INVENTORY] = Action { "interactive" };

    input_validator_.registerSchema(input_schema);
    output_validator_.registerSchema(output_schema);
}

CthunClient::DataContainer Inventory::callAction(
                                const std::string& action_name,
                                const CthunClient::ParsedChunks& parsed_chunks) {
    std::ostringstream fact_stream;
    CthunClient::DataContainer data {};

    facter::facts::collection facts;
    facts.add_default_facts();
    facts.write(fact_stream, facter::facts::format::json);

    LOG_TRACE("facts: %1%", fact_stream.str());
    data.set<std::string>("facts", fact_stream.str());

    return data;
}

}  // namespace Modules
}  // namespace CthunAgent
