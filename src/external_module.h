#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include "src/module.h"
#include "src/data_container.h"
#include "src/configuration.h"
#include "src/thread_container.h"

#include <rapidjson/document.h>

#include <map>
#include <string>
#include <vector>
#include <thread>

namespace CthunAgent {

class ExternalModule : public Module {
  public:
    /// Throw a module_error in case if fails to load the external
    /// module or if its metadata is invalid.
    explicit ExternalModule(const std::string& path);

    /// Call the specified action to execute the request.
    /// Throw a message_processing_error in case the action fails or
    /// if it returns an invalid output.
    DataContainer callAction(const std::string& action_name,
                             const Message& request);

    // This is public for test purposes
    DataContainer callBlockingAction(const std::string& action_name,
                                     const Message& request);

    // Public (as above); also passing the job_id for the same reason.
    DataContainer executeDelayedAction(const std::string& action_name,
                                       const Message& request,
                                       const std::string& job_id);

  private:
    /// Directory where the results of delayed actions will be stored
    std::string spool_dir_;

    /// The path of the module file
    const std::string path_;

    /// Manages the lifecycle of delayed action threads
    ThreadContainer thread_container_;

    const DataContainer validateModuleAndGetMetadata_();
    void validateAndDeclareAction_(const DataContainer& action);
};

}  // namespace CthunAgent

#endif  // SRC_EXTERNAL_MODULE_H_
