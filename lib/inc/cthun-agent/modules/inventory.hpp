#ifndef SRC_MODULES_INVENTORY_H_
#define SRC_MODULES_INVENTORY_H_

#include <cthun-agent/module.hpp>

namespace CthunAgent {
namespace Modules {

class Inventory : public CthunAgent::Module {
  public:
    Inventory();
    CthunClient::DataContainer callAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_INVENTORY_H_
