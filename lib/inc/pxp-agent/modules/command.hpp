#ifndef SRC_MODULES_COMMAND_H_
#define SRC_MODULES_COMMAND_H_

#include <pxp-agent/module.hpp>
#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/util/bolt_module.hpp>

namespace PXPAgent {
namespace Modules {

class Command : public PXPAgent::Util::BoltModule {
    public:
        Command(const boost::filesystem::path& exec_prefix, std::shared_ptr<ResultsStorage> storage);

    private:
        ActionResponse callAction(const ActionRequest& request) override;
};

}  // namespace Modules
}  // namespace PXPAgent

#endif  // SRC_MODULES_COMMAND_H_
