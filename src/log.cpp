#include "src/log.h"

// TODO(ale): enable compiler warnings

// boost includes are not always warning-clean. Disable warnings that
// cause problems before including the headers, then re-enable the warnings.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"

#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions/attr.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/attributes/current_thread_id.hpp>

#include <vector>
#include <unistd.h>

#pragma GCC diagnostic pop

namespace CthunAgent {

bool Log::is_log_enabled(const std::string &logger, Log::log_level level) {
    // If the severity_logger returns a record for the specified
    // level, logging is enabled. Otherwise it isn't.
    // This is the guard call used in BOOST_LOG_SEV; see
    // www.boost.org/doc/libs/1_54_0/libs/log/doc/html/log/detailed/sources.html
    boost::log::sources::severity_logger<Log::log_level> slg;
    return (slg.open_record(boost::log::keywords::severity = level) ? true : false);
}

void Log::configure_logging(Log::log_level level, std::ostream &dst) {
    // Set filtering based on log_level (info, warning, debug, etc).
    auto sink = boost::log::add_console_log(dst, boost::log::keywords::auto_flush = true);

    sink->set_formatter(boost::log::expressions::stream
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
            "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << " " << boost::log::expressions::attr<
            boost::log::attributes::current_thread_id::value_type>("ThreadID")
        << " " << std::left << std::setfill(' ') << std::setw(5)
        << Log::log_level_attr << " ["
        << Log::namespace_attr << ":"
        << boost::log::expressions::smessage);

    sink->set_filter(log_level_attr >= level);
    boost::log::add_common_attributes();
}

std::ostream& Log::operator<<(std::ostream& strm, Log::log_level level) {
    std::vector<std::string> levels {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
    if (static_cast<size_t>(level) < levels.size()) {
        strm << levels[static_cast<size_t>(level)];
    } else {
        strm << static_cast<int>(level);
    }
    return strm;
}

void Log::log(const std::string &logger, Log::log_level level, int line_num,
              boost::format& message) {
    log(logger, level, line_num, message.str());
}

//
// POSIX implementation of log()
//

// HERE(ale): we don't have the Win32 implementation; see cfacter

static std::string cyan(std::string const& message) {
    return "\33[0;36m" + message + "\33[0m";
}

static std::string green(std::string const& message) {
    return "\33[0;32m" + message + "\33[0m";
}

static std::string yellow(std::string const& message) {
    return "\33[0;33m" + message + "\33[0m";
}

static std::string red(std::string const& message) {
    return "\33[0;31m" + message + "\33[0m";
}

void Log::log(const std::string &logger, Log::log_level level, int line_num,
              std::string const& message) {
    boost::log::sources::severity_logger<Log::log_level> slg;
    slg.add_attribute("Namespace",
                      boost::log::attributes::constant<std::string>(logger));

    static bool color = isatty(fileno(stderr));
    if (!color) {
        BOOST_LOG_SEV(slg, level) << message;
        return;
    }

    switch (level) {
        case Log::log_level::trace:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << cyan(message);
            break;
        case Log::log_level::debug:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << cyan(message);
            break;
        case Log::log_level::info:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << green(message);
            break;
        case Log::log_level::warning:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << yellow(message);
            break;
        case Log::log_level::error:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << red(message);
            break;
        case Log::log_level::fatal:
            BOOST_LOG_SEV(slg, level) << line_num << "] - " << red(message);
            break;
        default:
            BOOST_LOG_SEV(slg, level) << line_num << "] - "
                                      << "Invalid logging level used.";
            break;
    }
}

}  // namespace CthunAgent
