#pragma once

#include <vector>
#include <string>
#include <memory>
#include <boost/filesystem.hpp>

namespace PXPAgent {
namespace Util {

class Purgeable {
  public:
    Purgeable() {}
    Purgeable(std::string ttl) : ttl_(std::move(ttl)) {}

    const std::string& get_ttl() const
    {
        return ttl_;
    }

    virtual unsigned int purge(
        const std::string& ttl,
        std::vector<std::string> ongoing_transactions,
        std::function<void(const std::string& dir_path)> purge_callback = nullptr) = 0;

  protected:
    static void defaultDirPurgeCallback(const std::string& dir_path)
    {
        boost::filesystem::remove_all(dir_path);
    }

  private:
    std::string ttl_;
};

}  // namespace Util
}  // namespace PXPAgent
