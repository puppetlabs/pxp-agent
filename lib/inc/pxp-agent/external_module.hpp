#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/thread_container.hpp>

#include <map>
#include <string>
#include <vector>

namespace PXPAgent {

class ExternalModule : public Module {
  public:
    /// Run the specified executable; its output must define the
    /// module by providing the metadata in JSON format.
    ///
    /// For each action of the module, the metadata must specify:
    ///     - name
    ///     - description
    ///     - input schema
    ///     - output schema
    ///
    /// After retrieving the metadata, validate it and, for each
    /// action defined in it, ensure that the specified input and
    /// output schemas are valid JSON schemas
    ///
    /// Throw a Module::LoadingError if: it fails to load the external
    /// module metadata; if the metadata is invalid; in case of
    /// invalid input or output schemas.
    explicit ExternalModule(const std::string& exec_path);

    explicit ExternalModule(const std::string& path,
                            const lth_jc::JsonContainer& config);

    /// The type of the module.
    Module::Type type() { return Module::Type::External; }

    /// In case a configuration schema has been registered for this
    /// module, validate configuration data.
    /// Throw a validation_error in case the configuration schema was
    /// not registered or in case of an invalid configuration data.
    void validateConfiguration();

    /// Writes the content of the specified out/err_file in,
    /// respectively, out/err_txt. Reads first err_file.
    /// Throws a ProcessingError in case it fails to read out_file.
    static void readNonBlockingOutcome(const ActionRequest& request,
                                       const std::string& out_file,
                                       const std::string& err_file,
                                       std::string& out_txt,
                                       std::string& err_txt);

  private:
    /// The path of the module file
    const std::string path_;

    /// Module configuration data
    lth_jc::JsonContainer config_;

    /// Metadata validator
    static const PCPClient::Validator metadata_validator_;

    const lth_jc::JsonContainer getMetadata();

    void registerConfiguration(const lth_jc::JsonContainer& config);

    void registerActions(const lth_jc::JsonContainer& metadata);

    void registerAction(const lth_jc::JsonContainer& action);

    /// Returns a string containing the arguments, in JSON format, for
    /// the requested action.
    /// The arguments of the PXP request will be added to an "input"
    /// entry.
    /// In case a configuration file was previously loaded for this
    /// action, its content will be added to a "configuration" entry.
    /// If the request's type is RequestType::NonBlocking, the paths
    /// to the output files will be added to an "output_files" entry.
    std::string getActionArguments(const ActionRequest& request);

    /// Log information about the outcome of the performed action
    /// while checking the exit code and validating the JSON format
    /// of the output.
    /// Returns an ActionOutcome object.
    /// Throws a ProcessingError in case of invalid output.
    ActionOutcome processRequestOutcome(const ActionRequest& request,
                                        int exit_code,
                                        std::string& out_txt,
                                        std::string& err_txt);

    ActionOutcome callBlockingAction(const ActionRequest& request);

    ActionOutcome callNonBlockingAction(const ActionRequest& request);

    ActionOutcome callAction(const ActionRequest& request);
};

}  // namespace PXPAgent

#endif  // SRC_EXTERNAL_MODULE_H_
