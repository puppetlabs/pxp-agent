#ifndef SRC_ACTION_H_
#define SRC_ACTION_H_

#include <valijson/schema.hpp>

namespace puppetlabs {
namespace cthun {

class Action {
public:
    valijson::Schema input_schema;
    valijson::Schema output_schema;
};

}
}

#endif  // SRC_ACTION_H_
