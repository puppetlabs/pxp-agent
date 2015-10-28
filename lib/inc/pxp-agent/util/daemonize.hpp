#ifndef SRC_AGENT_UTIL_DAEMONIZE_HPP_
#define SRC_AGENT_UTIL_DAEMONIZE_HPP_

#ifndef _WIN32
#include <pxp-agent/util/posix/pid_file.hpp>
#include <memory>
#endif
#include <pxp-agent/export.h>

namespace PXPAgent {
namespace Util {

#ifdef _WIN32
LIBPXP_AGENT_EXPORT void daemonize();
LIBPXP_AGENT_EXPORT void daemon_cleanup();
#else
LIBPXP_AGENT_EXPORT std::unique_ptr<PIDFile> daemonize();
#endif

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_AGENT_UTIL_DAEMONIZE_HPP_
