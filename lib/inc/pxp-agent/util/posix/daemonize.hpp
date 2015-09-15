#ifndef SRC_AGENT_UTIL_POSIX_DAEMONIZE_HPP_
#define SRC_AGENT_UTIL_POSIX_DAEMONIZE_HPP_

#include <pxp-agent/util/posix/pid_file.hpp>

#include <sys/stat.h>

#include <string>
#include <memory>

namespace PXPAgent {
namespace Util {

std::unique_ptr<PIDFile> daemonize();

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_AGENT_UTIL_POSIX_DAEMONIZE_HPP_
