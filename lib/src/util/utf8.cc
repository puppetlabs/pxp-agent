#include <rapidjson/rapidjson.h>
#if RAPIDJSON_MAJOR_VERSION > 1 || RAPIDJSON_MAJOR_VERSION == 1 && RAPIDJSON_MINOR_VERSION >= 1
// Header for StringStream was added in rapidjson 1.1 in a backwards incompatible way.
#include <rapidjson/stream.h>
#endif

#include <string>

namespace PXPAgent {
namespace Util {
    bool isValidUTF8(std::string &s) {
        rapidjson::StringStream source(s.data());
        rapidjson::InsituStringStream target(&s[0]);

        target.PutBegin();
        while (source.Tell() < s.size()) {
            if (!rapidjson::UTF8<char>::Validate(source, target)) {
                return false;
            }
        }
        return true;
    }
}  // namespace Util
}  // namespace PXPAgent
