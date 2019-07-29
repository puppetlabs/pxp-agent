#ifndef SRC_UTIL_BOLT_HELPERS_HPP_
#define SRC_UTIL_BOLT_HELPERS_HPP_

#include <leatherman/locale/locale.hpp>
#include <leatherman/curl/client.hpp>
#include <leatherman/json_container/json_container.hpp>

#include <tuple>

namespace PXPAgent {
namespace Util {
  boost::filesystem::path downloadFileFromMaster(const std::vector<std::string>& master_uris,
                                                uint32_t connect_timeout,
                                                uint32_t timeout,
                                                leatherman::curl::client& client,
                                                const boost::filesystem::path& cache_dir,
                                                const boost::filesystem::path& destination,
                                                const leatherman::json_container::JsonContainer& file);

  std::tuple<bool, std::string> downloadFileWithCurl(const std::vector<std::string>& master_uris,
                                                    uint32_t connect_timeout_s,
                                                    uint32_t timeout_s,
                                                    leatherman::curl::client& client,
                                                    const boost::filesystem::path& file_path,
                                                    const leatherman::json_container::JsonContainer& uri);
  std::string createUrlEndpoint(const leatherman::json_container::JsonContainer& uri);
  std::string calculateSha256(const std::string& path);
  void createDir(const boost::filesystem::path& dir);
  void createSymLink(const boost::filesystem::path& destination, const boost::filesystem::path& source);

}  // namespace Util
}  // namespace PXPAgent

#endif  // SRC_UTIL_BOLT_HELPERS_HPP_
