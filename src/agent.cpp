#include "agent.h"
#include "modules/echo.h"
#include "external_module.h"
#include "log.h"
#include "schemas.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace CthunAgent {

Agent::Agent() {
    // declare internal modules
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);

    // load external modules
    boost::filesystem::path module_path { "modules" };
    boost::filesystem::directory_iterator end;

    for (auto file = boost::filesystem::directory_iterator(module_path); file != end; ++file) {
        if (!boost::filesystem::is_directory(file->status())) {
            BOOST_LOG_TRIVIAL(info) << file->path().string();

            try {
                ExternalModule* external = new ExternalModule(file->path().string());
                modules_[external->name] = std::unique_ptr<Module>(external);
            } catch (...) {
                BOOST_LOG_TRIVIAL(error) << "Error loading " << file->path().string();
            }
        }
    }
}

void Agent::run(std::string module, std::string action) {
    Module* the_module = modules_[module].get();

    Json::Reader reader;
    Json::Value input;

    BOOST_LOG_TRIVIAL(info) << "loading stdin";

    if (!reader.parse(std::cin, input)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        return;
    }

    Json::Value output;
    the_module->call_action(action, input, output);
    BOOST_LOG_TRIVIAL(info) << output.toStyledString();
}


}  // namespace CthunAgent
