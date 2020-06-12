#ifndef SRC_UTIL_MODULE_CACHE_DIR_HPP_
#define SRC_UTIL_MODULE_CACHE_DIR_HPP_

#include <cpp-pcp-client/util/thread.hpp>
#include <leatherman/curl/client.hpp>
#include <leatherman/json_container/json_container.hpp>
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
      boost::filesystem::path getCachedFile(const std::vector<std::string>& master_uris,
                                      uint32_t connect_timeout,
                                      uint32_t timeout,
                                      leatherman::curl::client& client,
                                      const boost::filesystem::path& cache_dir,
                                      leatherman::json_container::JsonContainer& file);

      boost::filesystem::path downloadFileFromMaster(const std::vector<std::string>& master_uris,
                                                    uint32_t connect_timeout,
                                                    uint32_t timeout,
                                                    leatherman::curl::client& client,
                                                    const boost::filesystem::path& cache_dir,
                                                    const boost::filesystem::path& destination,
                                                    const leatherman::json_container::JsonContainer& file);

      unsigned int purgeCache(const std::string& ttl,
                              std::vector<std::string> ongoing_transactions,
                              std::function<void(const std::string& dir_path)> purge_callback);

      std::string cache_dir_;
      std::string purge_ttl_;

    private:
      std::tuple<bool, std::string> downloadFileWithCurl(const std::vector<std::string>& master_uris,
                                                        uint32_t connect_timeout_s,
                                                        uint32_t timeout_s,
                                                        leatherman::curl::client& client,
                                                        const boost::filesystem::path& file_path,
                                                        const leatherman::json_container::JsonContainer& uri);

      std::string createUrlEndpoint(const leatherman::json_container::JsonContainer& uri);
      std::string calculateSha256(const std::string& path);
      PCPClient::Util::mutex cache_purge_mutex_;
      PCPClient::Util::mutex curl_mutex_;
  };
}
#endif
