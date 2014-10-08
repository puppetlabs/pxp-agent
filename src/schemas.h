#ifndef SRC_SCHEMAS_H_
#define SRC_SCHEMAS_H_

#include <json/json.h>
#include <valijson/schema.hpp>

namespace Cthun {
namespace Agent {

class Schemas {
  public:
    static bool validate(const Json::Value& document,
                         const valijson::Schema& schema,
                         std::vector<std::string> &errors);
    static valijson::Schema external_action_metadata();
    static valijson::Schema network_message();
    static valijson::Schema cnc_data();
};

}  // namespace Agent
}  // namespace Cthun

#endif  // SRC_SCHEMAS_H_
