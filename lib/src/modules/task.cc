#include <pxp-agent/modules/task.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;

Task::Task(const fs::path& modules_dir, const fs::path& spool_dir)
        : ExternalModule { (modules_dir/"task").string(), spool_dir.string() }
{
}

ExternalModule::execArgs Task::getExecArgs(std::string action)
{
    return { path_, { action } };
}

void Task::prepareAction(const ActionRequest& request)
{
}

}  // namespace Modules
}  // namespace PXPAgent
