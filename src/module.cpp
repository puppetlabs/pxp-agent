#include "module.h"
#include "log.h"

namespace CthunAgent {

void Module::call_action(std::string action, const Json::Value& input, Json::Value& output) {
    BOOST_LOG_TRIVIAL(info) << "Invoking native action " << action;
}


}
