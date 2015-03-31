#ifndef SRC_MODULES_ECHO_H_
#define SRC_MODULES_ECHO_H_

#include <cthun-agent/module.hpp>

namespace CthunAgent {
namespace Modules {

class Echo : public CthunAgent::Module {
  public:
    Echo();
    CthunClient::DataContainer callAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace Modules
}  // namespace CthunAgent

#endif  // SRC_MODULES_ECHO_H_
