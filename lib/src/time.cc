#include <pxp-agent/time.hpp>

#include <boost/date_time/gregorian/greg_date.hpp>

#include <algorithm>  // std::remove_if
#include <locale>  // std::is_digit

namespace PXPAgent {

namespace pt = boost::posix_time;
namespace greg = boost::gregorian;

Timestamp::Timestamp(const std::string& past_duration)
        : time_point { Timestamp::getPastInstant(past_duration) }
{
}

static void processDurationString(std::string& past_duration,
                                  int& value,
                                  char& suffix)
{
    if (past_duration.size() < 2)
        throw Timestamp::Error { "invalid duration string: " + past_duration };

    suffix = past_duration.back();
    past_duration.pop_back();

    try {
        value = std::stoi(past_duration);
    } catch (const std::invalid_argument& e) {
        throw Timestamp::Error { "invalid duration string: "
                                 + past_duration + suffix };
    }
}

pt::ptime Timestamp::getPastInstant(std::string past_duration)
{
    int value {};
    char suffix {};
    processDurationString(past_duration, value, suffix);
    auto instant = pt::microsec_clock::universal_time();

    switch (suffix) {
        case ('d'):
            instant = instant - greg::days(value);
            break;
        case ('h'):
            instant = instant - pt::hours(value);
            break;
        case ('m'):
            instant = instant - pt::minutes(value);
            break;
        default:
            throw Error { "invalid duration string: "+ past_duration + suffix };
    }

    return instant;
}

unsigned int Timestamp::getMinutes(std::string past_duration)
{
    int value {};
    char suffix {};
    processDurationString(past_duration, value, suffix);
    unsigned int num_minutes { 0 };

    switch (suffix) {
        case ('d'):
            num_minutes = value * 24 * 60;
            break;
        case ('h'):
            num_minutes = value * 60;
            break;
        case ('m'):
            num_minutes = value;
            break;
        default:
            throw Error { "invalid duration string: " + past_duration + suffix };
    }

    return num_minutes;
}

std::string Timestamp::convertToISO(std::string extended_ISO8601_time)
{
    if (extended_ISO8601_time.empty())
        throw Error { "empty time string" };

    if (extended_ISO8601_time.size() < 21 || extended_ISO8601_time.back() != 'Z')
        throw Error { "invalid time string: " + extended_ISO8601_time };

    // Transform 2016-02-18T19:40:49.711227Z in 20160218T194049.711227
    extended_ISO8601_time.pop_back();
    extended_ISO8601_time.erase(
        std::remove_if(
            extended_ISO8601_time.begin(),
            extended_ISO8601_time.end(),
            [](const char& c) { return c == '-' || c == ':'; }),
        extended_ISO8601_time.end());

    return extended_ISO8601_time;
}

bool Timestamp::isOlderThan(const std::string& extended_ISO8601_time)
{
    auto t_p = pt::from_iso_string(Timestamp::convertToISO(extended_ISO8601_time));
    return time_point < t_p;
}

}  // namespace PXPAgent
