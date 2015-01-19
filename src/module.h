#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include "src/action.h"
#include "src/data_container.h"

#include <map>
#include <string>

namespace CthunAgent {

class Module {
  public:
    std::string module_name;
    std::map<std::string, Action> actions;

    virtual DataContainer call_action(std::string action_name,
                                      const Message& request) = 0;

    virtual void call_delayed_action(std::string action_name,
                                     const Message& request,
                                     std::string job_id) = 0;

    /// Validate the json schemas of input and output.
    /// Execute the requested action for the particular module.
    /// Sets an error response in the referred output json object
    /// in case of unknown action or invalid schemas.
    DataContainer validate_and_call_action(std::string action_name,
                                           const Message& request,
                                           std::string job_id = "");
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
