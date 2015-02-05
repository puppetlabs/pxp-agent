#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include "src/action.h"
#include "src/data_container.h"
#include "src/message.h"

#include <map>
#include <string>

namespace CthunAgent {

class Module {
  public:
    std::string module_name;
    std::map<std::string, Action> actions;

    /// Performs the requested action.
    virtual DataContainer callAction(const std::string& action_name,
                                     const Message& request) = 0;

    /// Validate the json schemas of input and output.
    /// Start the requested action for the particular module.
    /// Return the output of the action as a DataContainer object.
    /// Throw a validation error in case of unknown action,
    /// invalid request input, or if the requested action provides an
    /// invalid output.
    DataContainer validateAndCallAction(const std::string& action_name,
                                        const Message& request);
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
