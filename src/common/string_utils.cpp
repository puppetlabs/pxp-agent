#include "src/common/string_utils.h"

#include <iostream>
#include <ctime>
#include <time.h>

namespace Cthun {
namespace Common {
namespace StringUtils {

std::string plural(int num_of_things) {
    return num_of_things > 1 ? "s" : "";
}

template<>
std::string plural<std::string>(std::vector<std::string> things) {
    return plural(things.size());
}

std::string getExpiryDatetime(int expiry_minutes) {
    struct tm expiry_time_info;
    char expiry_time_buffer[80];

    // Get current time and add the specified minutes
    time_t expiry_time { time(nullptr) };
    expiry_time += 60 * expiry_minutes;

    // Get local time structure
    localtime_r(&expiry_time, &expiry_time_info);

    // Return the formatted string
    if (strftime(expiry_time_buffer, 80, "%FT%TZ", &expiry_time_info) == 0) {
        // invalid buffer
        return "";
    }

    return std::string { expiry_time_buffer };
}

// TODO(ale): test on Linux
void displayProgress(double percent, int len, std::string status) {
    printf("%c[2K", 27);
    printf("\r");

    if (percent > 0) {
        std::string progress {};
        for (int idx = 0; idx < len; ++idx) {
            if (idx < static_cast<int>(len * percent)) {
                progress += "=";
            } else {
                progress += " ";
            }
        }
        std::cout << status << " [" << progress << "] "
                  << static_cast<int>(100 * percent) << "%";
        std::flush(std::cout);
    }
}

}  // namespace StringUtils
}  // namespace Common
}  // namespace Cthun
