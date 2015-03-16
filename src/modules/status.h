#ifndef SRC_MODULES_STATUS_H_
#define SRC_MODULES_STATUS_H_

#include "src/module.h"

namespace CthunAgent {
namespace Modules {

class Status : public CthunAgent::Module {
  public:
    Status();
    CthunClient::DataContainer callAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_STATUS_H_
