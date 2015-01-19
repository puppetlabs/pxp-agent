#ifndef SRC_SCHEMAS_H_
#define SRC_SCHEMAS_H_

#include <rapidjson/document.h>
#include <valijson/schema.hpp>

namespace CthunAgent {

class Schemas {
  public:
    static bool validate(const rapidjson::Value& document,
                         const valijson::Schema& schema,
                         std::vector<std::string> &errors);
    static valijson::Schema external_action_metadata();
    static valijson::Schema network_message();
    static valijson::Schema cnc_data();
};

}  // namespace CthunAgent

#endif  // SRC_SCHEMAS_H_
