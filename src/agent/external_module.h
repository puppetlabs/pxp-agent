#ifndef SRC_AGENT_EXTERNAL_MODULE_H_
#define SRC_AGENT_EXTERNAL_MODULE_H_

#include "src/agent/module.h"
#include "src/data_container.h"

#include <map>
#include <string>

namespace Cthun {
namespace Agent {

class ExternalModule : public Module {
  public:
    explicit ExternalModule(std::string path);
    DataContainer call_action(std::string action_name,
                     const Message& request,
                     const DataContainer& input);

    void call_delayed_action(std::string action_name,
                             const Message& request,
                             const DataContainer& input,
                             std::string job_id);
  private:
    std::string path_;
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_AGENT_EXTERNAL_MODULE_H_
