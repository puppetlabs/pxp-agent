#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/module.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>


#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.bolt_helpers"
#include <leatherman/logging/logging.hpp>

#include <openssl/evp.h>

namespace fs       = boost::filesystem;
namespace alg      = boost::algorithm;
namespace lth_loc  = leatherman::locale;

namespace PXPAgent {
namespace Util {

  void findExecutableAndArguments(const fs::path& file, Util::CommandObject& cmd)
  {
      auto builtin = BUILTIN_INTERPRETERS.find(file.extension().string());

      if (builtin != BUILTIN_INTERPRETERS.end()) {
          auto details = builtin->second(file.string());
          cmd.executable = details.first;
          // cmd.arguments may already have values, so instead of setting
          // cmd.arguments directly: fetch the prefix arguments (that should
          // go before the user provided ones), then insert the user provided
          // ones in to prefix_args to create a flattened vector that will be:
          // { prefix_args, cmd.arguments }
          std::vector<std::string> prefix_args = details.second;
          std::vector<std::string> provided_args = cmd.arguments;
          prefix_args.insert(prefix_args.end(), provided_args.begin(), provided_args.end());
          cmd.arguments = prefix_args;
      } else {
          cmd.executable = file.string();
      }
  }

  // NIX_DIR_PERMS is defined in pxp-agent/configuration
  #define NIX_DOWNLOADED_FILE_PERMS NIX_DIR_PERMS

  void createDir(const fs::path& dir) {
    fs::create_directories(dir);
    fs::permissions(dir, NIX_DIR_PERMS);
  }

  // It looks like there are ways for POSIX nodes to specify umask of the pxp-agent service, which means any file/symlink/dir
  // created with boost should match the umask of the service process. That means between the file being created and the call
  // to fs::permissions it looks like the user can still control the security of the files/symlinks/dirs.

  // For windows: all permissions are inherited by the containing directory, meaning there is no scenario where files/symlinks
  // will have perms more open than the containing directories.

  // The above info should mean the links are safe between symlink creation and permissions setting.
  void createSymLink(const fs::path& destination, const fs::path& source) {
    fs::create_symlink(destination, source);
    fs::permissions(destination, NIX_DIR_PERMS);
  }

  // Try to split the raw command text into executable and arguments, as leatherman::execution
  // expects, taking care to retain spaces within quoted executables or argument names.
  // This approach is borrowed from boost's `program_options::split_unix`.
  std::vector<std::string> splitArguments(const std::string& raw_arg_list) {
    boost::escaped_list_separator<char> e_l_s {
        "",     // don't parse any escaped characters here,
                //   just get quote-aware space-separated chunks
        " ",    // split fields on spaces
        "\"\'"  // double or single quotes will group multiple fields as one
    };
    boost::tokenizer<boost::escaped_list_separator<char>> tok(raw_arg_list, e_l_s);
    return { tok.begin(), tok.end() };
  }

}  // namespace Util
}  // namespace PXPAgent
