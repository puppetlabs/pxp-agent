#ifndef SRC_UTIL_MODULE_CACHE_DIR_HPP_
#define SRC_UTIL_MODULE_CACHE_DIR_HPP_

#include <cpp-pcp-client/util/thread.hpp>
#include <boost/filesystem/path.hpp>


namespace PXPAgent {

  class ModuleCacheDir {
    public:
      ModuleCacheDir() = delete;
      ModuleCacheDir(const ModuleCacheDir&) = delete;
      ModuleCacheDir& operator=(const ModuleCacheDir&) = delete;
      ModuleCacheDir(const std::string& cache_dir,
                     const std::string& cache_dir_purge_ttl);

      boost::filesystem::path createCacheDir(const std::string& sha256);

      unsigned int purgeCache(const std::string& ttl,
                              std::vector<std::string> ongoing_transactions,
                              std::function<void(const std::string& dir_path)> purge_callback);

      std::string cache_dir_;
      std::string purge_ttl_;

    private:
      PCPClient::Util::mutex cache_dir_mutex_;
  };

}
#endif
