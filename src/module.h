#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <map>
#include <string>
#include "action.h"

namespace CthunAgent {

class Module {
public:
    std::string name;
    std::map<std::string,Action> actions;
};

}  // namespace CthunAgent

#endif  // SRC_MODULE_H_
