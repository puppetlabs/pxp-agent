#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/thread_container.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/results_storage.hpp>

#include <map>
#include <string>
#include <vector>

namespace PXPAgent {

class ExternalModule : public Module {
  public:
    /// Time interval to wait after the external module completes,
    /// before retrieving its output
    static const int OUTPUT_DELAY_MS;

    /// Run the specified executable; its output must define the
    /// module by providing the metadata in JSON format.
    ///
    /// After retrieving the metadata, validate it and, for each
    /// action defined in it, ensure that the specified input and
    /// output schemas are valid JSON schemas
    ///
    /// Throw a Module::LoadingError if: it fails to load the external
    /// module metadata; if the metadata is invalid; in case of
    /// invalid input or output schemas.
    explicit ExternalModule(const std::string& exec_path,
                            const std::string& spool_dir);

    explicit ExternalModule(
        const std::string& path,
        const leatherman::json_container::JsonContainer& config,
        const std::string& spool_dir);

    /// The type of the module.
    ModuleType type() override { return ModuleType::External; }

    /// If a configuration schema has been registered for this module,
    /// validate configuration data. In that case, throw a
    /// PCPClient::validation_error for invalid configuration data.
    void validateConfiguration();

    /// Log information about the output of the performed action
    /// while validating the JSON format of the output.
    /// Update the metadata of the ActionResponse instance (the
    /// 'results_are_valid', 'status', and 'execution_error' entries
    /// will be set appropriately; 'end' will be set to the current
    /// time).
    /// This function does not throw a ProcessingError in case of
    /// invalid output on stdout; such failure is instead reported
    /// in the response object's metadata.
    static void processOutputAndUpdateMetadata(ActionResponse& response);

  private:
    /// The path of the module file
    const std::string path_;

    /// Module configuration data
    leatherman::json_container::JsonContainer config_;

    /// Results Storage
    ResultsStorage storage_;

    /// Metadata validator
    static const PCPClient::Validator metadata_validator_;

    const leatherman::json_container::JsonContainer getModuleMetadata();

    void registerConfiguration(
        const leatherman::json_container::JsonContainer& config);

    void registerActions(
        const leatherman::json_container::JsonContainer& metadata);

    void registerAction(
        const leatherman::json_container::JsonContainer& action);

    /// Returns a string containing the arguments, in JSON format, for
    /// the requested action.
    /// The arguments of the PXP request will be added to an "input"
    /// entry.
    /// In case a configuration file was previously loaded for this
    /// action, its content will be added to a "configuration" entry.
    /// If the request's type is RequestType::NonBlocking, the paths
    /// to the output files will be added to an "output_files" entry.
    std::string getActionArguments(const ActionRequest& request);

    ActionResponse callBlockingAction(const ActionRequest& request);

    /// Throws a ProcessingError in case the module fails to write
    /// the action output to file.
    ActionResponse callNonBlockingAction(const ActionRequest& request);

    ActionResponse callAction(const ActionRequest& request) override;
};

}  // namespace PXPAgent

#endif  // SRC_EXTERNAL_MODULE_H_
