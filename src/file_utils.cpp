#include "src/common/file_utils.h"
#include "src/common/log.h"

#include <wordexp.h>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

LOG_DECLARE_NAMESPACE("common.file_utils");

namespace Cthun {
namespace Common {
namespace FileUtils {

// HERE(ale): copied from Pegasus

// TODO(ale): look for boost alternatives

std::string expandAsDoneByShell(std::string txt) {
    // This will store the expansion outcome
    wordexp_t result;

    // Expand and check the success
    if (wordexp(txt.c_str(), &result, 0) != 0) {
        wordfree(&result);
        return "";
    }

    // Get the expanded text and free the memory
    std::string expanded_txt { result.we_wordv[0] };
    wordfree(&result);
    return expanded_txt;
}

bool fileExists(const std::string& file_path) {
    bool exists { false };
    if (file_path.empty()) {
        LOG_WARNING("file path is an empty string");
    } else {
        std::ifstream file_stream { file_path };
        exists = file_stream.good();
        file_stream.close();
    }
    return exists;
}

void removeFile(const std::string& file_path) {
    if (FileUtils::fileExists(file_path)) {
        if (std::remove(file_path.c_str()) != 0) {
            throw file_error { "failed to remove " + file_path };
        }
    }
}

void streamToFile(const std::string& text,
                  const std::string& file_path,
                  std::ios_base::openmode mode) {
    std::ofstream ofs;
    ofs.open(file_path, mode);
    if (!ofs.is_open()) {
        throw file_error { "failed to open " + file_path };
    }
    ofs << text;
}

void writeToFile(const std::string& text, const std::string& file_path) {
    streamToFile(text, file_path, std::ofstream::out
                                | std::ofstream::trunc);
}

bool createDirectory(const std::string& dirname) {
    boost::filesystem::path dir(dirname);
    if (boost::filesystem::create_directory(dir)) {
        return true;
    }

    return false;
}

std::string readFileAsString(std::string path) {
    std::string content = "";

    if (!fileExists(path)) {
        return content;
    }

    std::ifstream file { path };
    std::string buffer;

    while (std::getline(file, buffer)) {
        content += buffer;
        content.push_back('\n');
    }

    return content;
}

}  // namespace FileUtils
}  // namespace Common
}  // namespace Cthun
