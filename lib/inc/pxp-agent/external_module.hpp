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

    /// In case a configuration schema has been registered fot this
    /// module, validate and set the passed configuration data.
    /// Throw a validation_error in case the configuration schema was
    /// not registered or in case of an invalid configuration data.
    void validateAndSetConfiguration(const lth_jc::JsonContainer& config);

  private:
    /// The path of the module file
    const std::string path_;

    /// Module configuration data
    lth_jc::JsonContainer config_;

    /// Metadata validator
    static const PCPClient::Validator metadata_validator_;

    const lth_jc::JsonContainer getMetadata();

    void registerConfiguration(const lth_jc::JsonContainer& config);

    void registerAction(const lth_jc::JsonContainer& action);

    ActionOutcome callAction(const ActionRequest& reqeust);
};

}  // namespace PXPAgent

#endif  // SRC_EXTERNAL_MODULE_H_
