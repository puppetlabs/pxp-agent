#pragma once
#include <pxp-agent/external_module.hpp>
#include <pxp-agent/action_response.hpp>
#include <boost/filesystem/path.hpp>

namespace PXPAgent {
namespace Modules {

class Task : public PXPAgent::ExternalModule {
  public:
    // exec_path will be the install location for pxp-agent
    Task(const boost::filesystem::path& modules_dir,
         const boost::filesystem::path& spool_dir);

  protected:
    void prepareAction(const ActionRequest& request) override;
    execArgs getExecArgs(std::string action) override;
};

}  // namespace Modules
}  // namespace PXPAgent
