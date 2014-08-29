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
                modules_[external->name] = std::shared_ptr<Module>(external);
            } catch (...) {
                BOOST_LOG_TRIVIAL(error) << "Error loading " << file->path().string();
            }
        }
    }
}

void Agent::list_modules() {
    BOOST_LOG_TRIVIAL(info) << "Loaded modules:";
    for (auto module : modules_) {
        BOOST_LOG_TRIVIAL(info) << "   " << module.first;
        for (auto action : module.second->actions) {
            BOOST_LOG_TRIVIAL(info) << "       " << action.first;
        }
    }
}

void Agent::run(std::string module, std::string action) {
    list_modules();

    std::shared_ptr<Module> the_module = modules_[module];

    Json::Reader reader;
    Json::Value input;

    BOOST_LOG_TRIVIAL(info) << "loading stdin";

    if (!reader.parse(std::cin, input)) {
        BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
        return;
    }

    try {
        Json::Value output;
        the_module->validate_and_call_action(action, input, output);
        BOOST_LOG_TRIVIAL(info) << output.toStyledString();
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Badness occured";
    }
}

void Agent::send_login() {
    Json::Value login {};
    login["id"] = 1;
    login["version"] = "1";
    login["expires"] = "2014-08-28T17:01:05Z";
    login["sender"] = "localhost/agent";
    login["endpoints"] = Json::Value { Json::arrayValue };
    login["endpoints"][0] = "cth://server";
    login["hops"] = Json::Value { Json::arrayValue };
    login["data_schema"] = "http://puppetlabs.com/loginschema";
    login["data"]["type"] = "agent";
    login["data"]["user"] = "agent";


    BOOST_LOG_TRIVIAL(error) << login.toStyledString();

    valijson::Schema message_schema = Schemas::network_message();
    std::vector<std::string> errors;

    if (!Schemas::validate(login, message_schema, errors)) {
        BOOST_LOG_TRIVIAL(error) << "Validation failed";
        for (auto error : errors) {
            BOOST_LOG_TRIVIAL(error) << "    " << error;
        }
        throw "input schema mismatch";
    }

    client_.send(connection_, login.toStyledString());
}

void Agent::connect_and_run() {
    client_.onMessage = [this](std::string message) {
        BOOST_LOG_TRIVIAL(info) << "got message" << message;
    };

    client_.onOpen = [this](Cthun::Client::Connection_Handle opened) {
        connection_ = opened;
        send_login();
    };

    connection_ = client_.connect("ws://localhost:8080/cthun/");


    while(1) {
        sleep(10);
    }
}

}  // namespace CthunAgent
