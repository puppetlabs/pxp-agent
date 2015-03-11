#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include "src/module.h"
#include "src/configuration.h"
#include "src/thread_container.h"

#include <map>
#include <string>
#include <vector>
#include <thread>

namespace CthunAgent {

// TODO(ale): evaluate changing 'interactive' behaviour to 'blocking'

class ExternalModule : public Module {
  public:
    /// Run the specified executable; its output must define the
    /// module by providing its metadata in JSON format.
    ///
    /// For each action of the module, the metadata must specify:
    ///     - name
    ///     - description
    ///     - input schema
    ///     - output schema
    ///     - behaviour (expected string values: delayed, interactive)
    ///
    /// After retrieving the metadata, validate it and, for each
    /// action defined in it, do: 1) ensure that the specified input
    /// and output schemas are valid JSON schemas; 2) check the action
    /// behaviour.
    ///
    /// Throw a module_error if: if fails to load the external module
    /// metadata; if the metadata is invalid; in case of invalid input
    /// or output schemas; in case of an unknown behaviour.
    explicit ExternalModule(const std::string& exec_path);

    /// Call the specified action.
    /// Throw a request_processing_error in case it fails to execute
    /// the action or if the action returns an invalid output.
    CthunClient::DataContainer callAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);

    // Execute a blocking action; return the output as a DataContainer
    // object once the action is completed.
    // This function is public for test purposes.
    CthunClient::DataContainer callBlockingAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks);

    // Start a delayed action and return immediately the data
    // containing the id of the particular action job.
    // This function is public for test purposes; also, passing the
    // job_id as an argument for the same reason.
    CthunClient::DataContainer executeDelayedAction(
                    const std::string& action_name,
                    const CthunClient::ParsedChunks& parsed_chunks,
                    const std::string& job_id);

  private:
    /// Directory where the results of delayed actions will be stored
    const std::string spool_dir_;

    /// The path of the module file
    const std::string path_;

    /// Manages the lifecycle of delayed action threads
    ThreadContainer thread_container_;

    /// Validator
    static const CthunClient::Validator metadata_validator_;

    const CthunClient::DataContainer getMetadata_();
    void registerAction_(const CthunClient::DataContainer& action);
};

}  // namespace CthunAgent

#endif  // SRC_EXTERNAL_MODULE_H_
