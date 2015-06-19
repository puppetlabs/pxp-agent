#ifndef SRC_EXTERNAL_MODULE_H_
#define SRC_EXTERNAL_MODULE_H_

#include <cthun-agent/module.hpp>
#include <cthun-agent/configuration.hpp>
#include <cthun-agent/thread_container.hpp>

#include <map>
#include <string>
#include <vector>
#include <thread>

namespace CthunAgent {

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
    /// Throw a module_error if: if fails to load the external module
    /// metadata; if the metadata is invalid; in case of invalid input
    /// or output schemas.
    explicit ExternalModule(const std::string& exec_path);

  private:
    /// The path of the module file
    const std::string path_;

    /// Validator
    static const CthunClient::Validator metadata_validator_;

    const CthunClient::DataContainer getMetadata();

    void registerAction(const CthunClient::DataContainer& action);

    ActionOutcome callAction(const ActionRequest& reqeust);
};

}  // namespace CthunAgent

#endif  // SRC_EXTERNAL_MODULE_H_
