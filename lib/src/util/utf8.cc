#include <rapidjson/rapidjson.h>
#if RAPIDJSON_MAJOR_VERSION > 1 || RAPIDJSON_MAJOR_VERSION == 1 && RAPIDJSON_MINOR_VERSION >= 1
// Header for StringStream was added in rapidjson 1.1 in a backwards incompatible way.
#include <rapidjson/stream.h>
#endif

#include <algorithm>
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

        // rapidjson::UTF8<char>::Validate accepts null bytes as valid UTF-8.
        // They technically are valid since they're the null character. But
        // null characters aren't valid in a string, so we want to disallow
        // them regardless.
        bool has_null = std::any_of(s.begin(), s.end(), [](char c) {
                            return c == 0;
                        });
        return !has_null;
    }
}  // namespace Util
}  // namespace PXPAgent
