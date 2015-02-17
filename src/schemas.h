#ifndef SRC_SCHEMAS_H_
#define SRC_SCHEMAS_H_

#include <rapidjson/document.h>
#include <valijson/schema.hpp>

#include <string>
#include <map>

namespace CthunAgent {

static const std::string CTHUN_LOGIN_SCHEMA { "http://puppetlabs.com/loginschema" };
static const std::string CTHUN_REQUEST_SCHEMA { "http://puppetlabs.com/cnc_request" };
static const std::string CTHUN_RESPONSE_SCHEMA { "http://puppetlabs.com/cnc_response" };

static const std::string CTHUN_JSON_FORMAT { "json" };

enum class CthunContentFormat { json };

static std::map<const std::string, const CthunContentFormat> SchemaFormat {
    { CTHUN_LOGIN_SCHEMA, CthunContentFormat::json },
    { CTHUN_REQUEST_SCHEMA, CthunContentFormat::json },
    { CTHUN_RESPONSE_SCHEMA, CthunContentFormat::json }
};

class Schemas {
  public:
    static bool validate(const rapidjson::Value& document,
                         const valijson::Schema& schema,
                         std::vector<std::string> &errors);

    static valijson::Schema getExternalActionMetadataSchema();
    static valijson::Schema getEnvelopeSchema();
    static valijson::Schema getDataSchema();

    // TODO(ale): debug chunk content validation schema
};

}  // namespace CthunAgent

#endif  // SRC_SCHEMAS_H_
