#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <map>
#include <string>
#include "action.h"

namespace puppetlabs {
namespace cthun {

class Module {
public:
    std::string name;
    std::map<std::string,Action> actions;
};


}  // namespace cthun
}  // namespace puppetlabs


#endif  // SRC_MODULE_H_
