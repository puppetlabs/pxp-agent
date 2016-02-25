#ifndef SRC_AGENT_TIME_HPP
#define SRC_AGENT_TIME_HPP

#include <boost/date_time/posix_time/posix_time.hpp>

#include <string>
#include <stdexcept>

// TODO(ale): consider moving some of this to leatherman

namespace PXPAgent {

// Holds a time point reference to a past instant and allows comparing
// it to timestamps in ISO8601 format
class Timestamp {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    boost::posix_time::ptime time_point;

    // If the specified time interval is not in the following format,
    // the ctor throws an Error: <integer value><suffix>
    // Possible suffixes (ex. "14d"):
    //      `d`  - days
    //      `h`  - hours
    //      `m`  - minutes
    Timestamp(const std::string& past_duration);

    // Returns a ptime time point for a past instant that is old
    // as specified
    // Throws an Error in case past_duration is not in a valid format,
    // as described mentioned above
    static boost::posix_time::ptime getPastInstant(std::string past_duration);

    // Returns the number of minutes out of the specified duration
    // Throws an Error in case past_duration is not in a valid format,
    // as described mentioned above
    static unsigned int getMinutes(std::string past_duration);

    // Throws an Error in case the specified time string is not in
    // the extended ISO format (refer to boost date_time docs)
    static std::string convertToISO(std::string extended_ISO8601_time);

    // Throws an Error in case it fails to create a time point from
    // the specified extended ISO date time string
    bool isNewerThan(const std::string& extended_ISO8601_time);
};

}  // namespace PXPAgents

#endif  // SRC_AGENT_TIME_HPP
