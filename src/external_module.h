#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include "src/module.h"
#include "src/data_container.h"

#include <rapidjson/document.h>

#include <map>
#include <string>

namespace CthunAgent {

class ExternalModule : public Module {
  public:
    // Throws a module_error in case if fails to load the external
    // module or if its metadata is invalid.
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

}  // namespace CthunAgent

#endif  // SRC_EXTERNAL_MODULE_H_
