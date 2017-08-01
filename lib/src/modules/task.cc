#include <pxp-agent/modules/task.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.modules.task"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;

// TODO: construct and pass metadata to a new ExternalModule constructor, rather than calling `task metadata`
// TODO: how do we get the path to task.exe if it's co-located with pxp-agent, rather than in modules dir?
Task::Task(const fs::path& modules_dir, const fs::path& spool_dir)
        : ExternalModule { (modules_dir/"task").string(), spool_dir.string() }
{
}

ExternalModule::execArgs Task::getExecArgs(std::string action)
{
    return { path_, { action } };
}

// TODO: move environment generation for input to a new virtual method?

// TODO: move local task lookup from task.cc to here
//       handle returning an error if no module is found
void Task::prepareAction(const ActionRequest& request)
{
}

}  // namespace Modules
}  // namespace PXPAgent
