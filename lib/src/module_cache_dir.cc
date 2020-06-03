#include <pxp-agent/module_cache_dir.hpp>
#include <pxp-agent/module.hpp>
#include <pxp-agent/util/purgeable.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>

#include <leatherman/locale/locale.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/system/error_code.hpp>

#include <openssl/evp.h>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.util.module_cache_dir"
#include <leatherman/logging/logging.hpp>

namespace alg         = boost::algorithm;
namespace fs          = boost::filesystem;
namespace boost_error = boost::system::errc;
namespace pcp_util    = PCPClient::Util;
namespace lth_curl    = leatherman::curl;
namespace lth_loc     = leatherman::locale;
namespace lth_file    = leatherman::file_util;
namespace lth_jc      = leatherman::json_container;

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
    pcp_util::lock_guard<pcp_util::mutex> purge_lock { cache_purge_mutex_ };
    auto file_cache_dir = static_cast<fs::path>(cache_dir_) / sha256;
    try {
      Util::createDir(file_cache_dir);
      fs::last_write_time(file_cache_dir, time(nullptr));
    } catch (fs::filesystem_error& e) {
      auto err_code = e.code();
      if (err_code == boost_error::no_such_file_or_directory) {
          throw Module::ProcessingError(lth_loc::format("No such file or directory: {1}", e.path1()));
      } else {
          throw Module::ProcessingError(lth_loc::format("Failed to create cache dir {1} to download file to: {2}", file_cache_dir, e.what()));
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
      // Lambda function
      [&](std::string const& sub_dir) -> bool {
        fs::path dir_path { sub_dir };
        LOG_TRACE("Inspecting '{1}' for purging", sub_dir);

        boost::system::error_code ec;
        pcp_util::lock_guard<pcp_util::mutex> purge_lock { cache_purge_mutex_ };
        auto last_update = fs::last_write_time(dir_path, ec);
        if (ec) {
          LOG_ERROR("Failed to remove '{1}': {2}", sub_dir, ec.message());
        } else if (ts.isNewerThan(last_update)) {
          LOG_TRACE("Removing '{1}'", sub_dir);

          try {
            purge_callback(dir_path.string());
            num_purged_dirs++;
          } catch (const std::exception& e) {
            LOG_ERROR("Failed to remove '{1}': {2}", sub_dir, e.what());
          }
        }
        return true;  // Return from Lamda function passed to lth_file::each_subdirectory
      });

    LOG_INFO(lth_loc::format_n(
      // LOCALE: info
      "Removed {1} directory from '{2}'",
      "Removed {1} directories from '{2}'",
      num_purged_dirs, num_purged_dirs, cache_dir_));
    return num_purged_dirs;
  }

  // NIX_DIR_PERMS is defined in pxp-agent/configuration
  #define NIX_DOWNLOADED_FILE_PERMS NIX_DIR_PERMS

  // Computes the sha256 of the file denoted by path. Assumes that
  // the file designated by "path" exists.
  std::string ModuleCacheDir::calculateSha256(const std::string& path) {
    auto mdctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    {
      constexpr std::streamsize CHUNK_SIZE = 0x8000;  // 32 kB
      char buffer[CHUNK_SIZE];
      boost::nowide::ifstream ifs(path, std::ios::binary);

      while (ifs.read(buffer, CHUNK_SIZE)) {
        EVP_DigestUpdate(mdctx, buffer, CHUNK_SIZE);
      }
      if (!ifs.eof()) {
        EVP_MD_CTX_destroy(mdctx);
        throw Module::ProcessingError(lth_loc::format("Error while reading {1}", path));
      }
      EVP_DigestUpdate(mdctx, buffer, ifs.gcount());
    }

    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_destroy(mdctx);

    std::string md_value_hex;

    md_value_hex.reserve(2*md_len);
    // TODO use boost::algorithm::hex_lower and drop the std::transform below when we upgrade to boost 1.62.0 or newer
    alg::hex(md_value, md_value+md_len, std::back_inserter(md_value_hex));
    std::transform(md_value_hex.begin(), md_value_hex.end(), md_value_hex.begin(), ::tolower);

    return md_value_hex;
  }

  std::string ModuleCacheDir::createUrlEndpoint(const lth_jc::JsonContainer& uri) {
    std::string url = uri.get<std::string>("path");
    auto params = uri.getWithDefault<lth_jc::JsonContainer>("params", lth_jc::JsonContainer());
    if (params.empty()) {
      return url;
    }
    auto curl_handle = lth_curl::curl_handle();
    url += "?";
    for (auto& key : params.keys()) {
      auto escaped_key = std::string(lth_curl::curl_escaped_string(curl_handle, key));
      auto escaped_val = std::string(lth_curl::curl_escaped_string(curl_handle, params.get<std::string>(key)));
      url += escaped_key + "=" + escaped_val + "&";
    }
    // Remove trailing ampersand (&)
    url.pop_back();
    return url;
  }

  // Downloads the file at the specified url into the provided path.
  // The downloaded file's permissions will be set to rwx for user and rx for
  // group for non-Windows OSes.
  //
  // The method returns a tuple (success, err_msg). success is true if the file was downloaded;
  // false otherwise. err_msg contains the most recent http_file_download_exception's error
  // message; it is initially empty.
  std::tuple<bool, std::string> ModuleCacheDir::downloadFileWithCurl(const std::vector<std::string>& master_uris,
                                                                     uint32_t connect_timeout_s,
                                                                     uint32_t timeout_s,
                                                                     lth_curl::client& client,
                                                                     const fs::path& file_path,
                                                                     const lth_jc::JsonContainer& uri) {
    pcp_util::lock_guard<pcp_util::mutex> curl_lock { curl_mutex_ };
    auto endpoint = createUrlEndpoint(uri);
    std::tuple<bool, std::string> result = std::make_tuple(false, "");
    for (auto& master_uri : master_uris) {
      auto url = master_uri + endpoint;
      lth_curl::request req(url);

      // Request timeouts expect milliseconds.
      req.connection_timeout(connect_timeout_s*1000);
      req.timeout(timeout_s*1000);

      try {
        lth_curl::response resp;
        client.download_file(req, file_path.string(), resp, NIX_DOWNLOADED_FILE_PERMS);
        if (resp.status_code() >= 400) {
          throw lth_curl::http_file_download_exception(
            req,
            file_path.string(),
            lth_loc::format("{1} returned a response with HTTP status {2}. Response body: {3}", url, resp.status_code(), resp.body()));
        }
      } catch (lth_curl::http_file_download_exception& e) {
        // Server-side error, do nothing here -- we want to try the next master-uri.
        LOG_WARNING("Downloading the file from the master-uri '{1}' failed. Reason: {2}", master_uri, e.what());
        std::get<1>(result) = e.what();
      } catch (lth_curl::http_request_exception& e) {
        // For http_curl_setup and http_file_operation exceptions
        throw Module::ProcessingError(lth_loc::format("Downloading the file failed. Reason: {1}", e.what()));
      }

      if (fs::exists(file_path)) {
        std::get<0>(result) = true;
        return result;
      }
    }

    return result;
  }

  // Downloads a file if it does not already exist on the filesystem. A check is made
  // on the filesystem to determine if the file at destination already exists and if
  // it already matches the sha256 provided with the file. If the file already exists
  // the function immediately returns.
  //
  // If the file does not exist attempt to download with leatherman.curl. Once the
  // download finishes a sha256 check occurs to ensure file contents are correct. Then
  // the file is moved to destination with boost::filesystem::rename.
  fs::path ModuleCacheDir::downloadFileFromMaster(const std::vector<std::string>& master_uris,
                                                  uint32_t connect_timeout,
                                                  uint32_t timeout,
                                                  lth_curl::client& client,
                                                  const fs::path& cache_dir,
                                                  const fs::path& destination,
                                                  const lth_jc::JsonContainer& file) {
    auto filename = destination.filename();
    auto sha256 = file.get<std::string>("sha256");

    if (fs::exists(destination) && boost::to_upper_copy<std::string>(sha256) == boost::to_upper_copy<std::string>(calculateSha256(destination.string()))) {
      fs::permissions(destination, NIX_DOWNLOADED_FILE_PERMS);
      return destination;
    }

    if (master_uris.empty()) {
      throw Module::ProcessingError(lth_loc::format("Cannot download file. No master-uris were provided"));
    }

    auto tempname = cache_dir / fs::unique_path("temp_file_%%%%-%%%%-%%%%-%%%%");
    // Note that the provided tempname argument is a temporary file, call it "tempA".
    // Leatherman.curl during the download method will create another temporary file,
    // call it "tempB", to save the downloaded file's contents in chunks before
    // renaming it to "tempA." The rationale behind this solution is that:
    //    (1) After download, we still need to check "tempA" to ensure that its sha matches
    //    the provided sha. So the downloaded file is not quite a "valid" file after this
    //    method is called; it's still temporary.
    //
    //    (2) It somewhat simplifies error handling if multiple threads try to download
    //    the same file.
    auto download_result = downloadFileWithCurl(master_uris, connect_timeout, timeout, client, tempname, file.get<lth_jc::JsonContainer>("uri"));
    if (!std::get<0>(download_result)) {
      throw Module::ProcessingError(lth_loc::format(
        "Downloading file {1} failed after trying all the available master-uris. Most recent error message: {2}",
        filename,
        std::get<1>(download_result)));
    }

    if (sha256 != calculateSha256(tempname.string())) {
      fs::remove(tempname);
      throw Module::ProcessingError(lth_loc::format("The downloaded file {1} has a SHA that differs from the provided SHA", filename));
    }
    if (!fs::exists(destination.parent_path())) {
      Util::createDir(destination.parent_path());
    }
    try {
      fs::rename(tempname, destination);
    } catch (boost::filesystem::filesystem_error& fs_error) {
      // Catch EXDEV (tempname and destination on different filesystems) and attempt to retry the file
      // move with `copy` then `remove`
      // Note that EXDEV should be available on windows too: https://docs.microsoft.com/en-us/cpp/c-runtime-library/errno-constants?view=vs-2019
      if (fs_error.code().value() == EXDEV) {
        fs::copy(tempname, destination);
        fs::remove(tempname);
      } else {
        throw fs_error;
      }
    }
    return destination;
  }


  // Verify (this includes checking the SHA256 checksums) that a file is present
  // in the cache, downloading it if necessary.
  // Return the full path of the cached version of the file.
  fs::path ModuleCacheDir::getCachedFile(const std::vector<std::string>& master_uris,
                                         uint32_t connect_timeout,
                                         uint32_t timeout,
                                         lth_curl::client& client,
                                         const fs::path&   cache_dir,
                                         lth_jc::JsonContainer& file) {
      LOG_DEBUG("Verifying file based on {1}", file.toString());

      try {
          // files remain in the cache_dir rather than being written out to a destination
          // elsewhere on the filesystem.
          auto destination = cache_dir / fs::path(file.get<std::string>("filename")).filename();
          return downloadFileFromMaster(master_uris, connect_timeout, timeout, client, cache_dir, destination, file);
      } catch (Module::ProcessingError& e) {
          throw Module::ProcessingError { lth_loc::format("Failed to download file with: {1}", e.what()) };
      }
  }
}  // PXPAgent
