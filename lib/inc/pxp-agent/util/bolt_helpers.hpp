#ifndef SRC_UTIL_BOLT_HELPERS_HPP_
#define SRC_UTIL_BOLT_HELPERS_HPP_

#include <leatherman/locale/locale.hpp>
#include <pxp-agent/util/bolt_module.hpp>

#include <tuple>

namespace PXPAgent {
namespace Util {

  // Hard-code interpreters on Windows. On non-Windows, we still rely on permissions and #!
  const std::map<std::string, std::function<std::pair<std::string, std::vector<std::string>>(std::string)>> BUILTIN_INTERPRETERS {
  #ifdef _WIN32
      {".rb",  [](std::string filename) { return std::pair<std::string, std::vector<std::string>> {
          "ruby", { filename }
      }; }},
      {".pp",  [](std::string filename) { return std::pair<std::string, std::vector<std::string>> {
          "puppet", { "apply", filename }
      }; }},
      {".ps1", [](std::string filename) { return std::pair<std::string, std::vector<std::string>> {
          "powershell",
          { "-NoProfile", "-NonInteractive", "-NoLogo", "-ExecutionPolicy", "Bypass", "-File", filename }
      }; }}
  #endif
  };

  void findExecutableAndArguments(const boost::filesystem::path& file, Util::CommandObject& cmd);
  void createDir(const boost::filesystem::path& dir);
  void createSymLink(const boost::filesystem::path& destination, const boost::filesystem::path& source);
  std::vector<std::string> splitArguments(const std::string& raw_arg_list);
}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_UTIL_BOLT_HELPERS_HPP_
