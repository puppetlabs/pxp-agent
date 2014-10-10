#ifndef SRC_COMMON_STRINGUTILS_H_
#define SRC_COMMON_STRINGUTILS_H_

#include <string>
#include <vector>

namespace Cthun {
namespace Common {
namespace StringUtils {

/// Return the "s" string in case of more than one things,
/// an empty string otherwise.
std::string plural(int num_of_things);

template<typename T>
std::string plural(std::vector<T> things);

/// Adds the specified expiry_minutes to the current time and returns
/// the related date time string in UTC format.
/// Returns an empty string in case it fails to allocate the buffer.
std::string getExpiryDatetime(int expiry_minutes = 1);

/// Erases the current line on stdout, places the cursor at the
/// beginning of it, and displays a progress message.
void displayProgress(double percent, int len,
                     std::string status = "in progress");

}  // namespace StringUtils
}  // namespace Common
}  // namespace Cthun

#endif  // SRC_COMMON_STRINGUTILS_H_
