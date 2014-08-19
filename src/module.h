#ifndef SRC_MODULE_H_
#define SRC_MODULE_H_

#include <map>
#include <string>
#include <valijson/schema.hpp>

namespace puppetlabs {
namespace cthun {

class Module {
public:
    std::string name;
    std::map<std::string,valijson::Schema> actions;
};


}  // namespace cthun
}  // namespace puppetlabs


#endif  // SRC_MODULE_H_
