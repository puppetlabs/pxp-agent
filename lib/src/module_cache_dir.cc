#include <pxp-agent/module_cache_dir.hpp>
#include <pxp-agent/module.hpp>
#include <pxp-agent/util/purgeable.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>
#include <boost/system/error_code.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.module_cache_dir"
#include <leatherman/logging/logging.hpp>

namespace fs          = boost::filesystem;
namespace pcp_util    = PCPClient::Util;
namespace lth_loc     = leatherman::locale;
namespace lth_file    = leatherman::file_util;
namespace boost_error = boost::system::errc;

namespace PXPAgent {
  ModuleCacheDir::ModuleCacheDir(const std::string& cache_dir,
                                 const std::string& cache_dir_purge_ttl) :
    cache_dir_ { cache_dir },
    purge_ttl_ { cache_dir_purge_ttl }
  {}

  // Creates the <cache_dir>/<sha256> directory (and parent dirs), ensuring that its permissions are readable by
  // the PXP agent owner/group (for unix OSes), writable for the PXP agent owner,
  // and executable by both PXP agent owner and group. Returns the path to this directory.
  // Note that the last modified time of the directory is updated, and that this routine
  // will not fail if the directory already exists.
  fs::path ModuleCacheDir::createCacheDir(const std::string& sha256) {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { cache_dir_mutex_ };
    auto file_cache_dir = static_cast<fs::path>(cache_dir_) / sha256;
    try {
      Util::createDir(file_cache_dir);
      fs::last_write_time(file_cache_dir, time(nullptr));
    } catch (fs::filesystem_error& e) {
      auto err_code = e.code();
      if (err_code == boost_error::no_such_file_or_directory) {
          throw Module::ProcessingError(lth_loc::format("No such file or directory: {1}", e.path1()));
      } else {
          throw Module::ProcessingError(lth_loc::format("Failed to create cache dir to download file to: {1}", e.what()));
      }
    }
    return file_cache_dir;
  }

  unsigned int ModuleCacheDir::purgeCache(const std::string& ttl,
                                          std::vector<std::string> ongoing_transactions,
                                          std::function<void(const std::string& dir_path)> purge_callback)
  {
    unsigned int num_purged_dirs { 0 };
    Timestamp ts { ttl };

    LOG_INFO("About to purge cached files from '{1}'; TTL = {2}",
        cache_dir_, ttl);

    lth_file::each_subdirectory(
      cache_dir_,
      [&](std::string const& s) -> bool {
        fs::path dir_path { s };
        LOG_TRACE("Inspecting '{1}' for purging", s);

        boost::system::error_code ec;
        pcp_util::lock_guard<pcp_util::mutex> the_lock { cache_dir_mutex_ };
        auto last_update = fs::last_write_time(dir_path, ec);
        if (ec) {
          LOG_ERROR("Failed to remove '{1}': {2}", s, ec.message());
        } else if (ts.isNewerThan(last_update)) {
          LOG_TRACE("Removing '{1}'", s);

          try {
            purge_callback(dir_path.string());
            num_purged_dirs++;
          } catch (const std::exception& e) {
            LOG_ERROR("Failed to remove '{1}': {2}", s, e.what());
          }
        }

        return true;
      });

    LOG_INFO(lth_loc::format_n(
      // LOCALE: info
      "Removed {1} directory from '{2}'",
      "Removed {1} directories from '{2}'",
      num_purged_dirs, num_purged_dirs, cache_dir_));
    return num_purged_dirs;
  }
}  // PXPAgent
