#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/time.hpp>

#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.results_storage"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>  // std::find

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_file = leatherman::file_util;

static const std::string METADATA { "metadata" };
static const std::string STDOUT { "stdout" };
static const std::string STDERR { "stderr" };
static const std::string EXITCODE { "exitcode" };
static const std::string PID { "pid" };

ResultsStorage::ResultsStorage(const std::string& spool_dir)
        : spool_dir_path_ { spool_dir }
{
}

bool ResultsStorage::find(const std::string& transaction_id)
{
    auto p = spool_dir_path_ / transaction_id;
    return fs::exists(p) && fs::is_directory(p);
}

static void writeMetadata(const std::string& txt, const std::string& file_path) {
    try {
        lth_file::atomic_write_to_file(txt, file_path);
    } catch (const std::exception& e) {
        std::string err_msg { "failed to write metadata: " };
        throw ResultsStorage::Error { err_msg + e.what() };
    }
}

void ResultsStorage::initializeMetadataFile(const std::string& transaction_id,
                                            const lth_jc::JsonContainer& metadata)
{
    auto results_path = spool_dir_path_ / transaction_id;

    if (!fs::exists(results_path)) {
        LOG_DEBUG("Creating results directory for the  transaction %1% in '%2%'",
                  transaction_id, results_path.string());
        try {
            fs::create_directories(results_path);
        } catch (const fs::filesystem_error& e) {
            throw ResultsStorage::Error {
                std::string("failed to create results directory: ") + e.what() };
        }
    }

    auto metadata_file = (results_path / METADATA).string();
    writeMetadata(metadata.toString() + "\n", metadata_file);
}

void ResultsStorage::updateMetadataFile(const std::string& transaction_id,
                                        const lth_jc::JsonContainer& metadata)
{
    if (!find(transaction_id))
        throw Error { "no results directory for the transaction "
                      + transaction_id };

    auto metadata_file = (spool_dir_path_ / transaction_id / METADATA).string();
    writeMetadata(metadata.toString() + "\n", metadata_file);
}

lth_jc::JsonContainer
ResultsStorage::getActionMetadata(const std::string& transaction_id)
{
    auto metadata_file = (spool_dir_path_ / transaction_id / METADATA).string();
    std::string metadata_txt {};

    if (!fs::exists(metadata_file))
        throw Error { "metadata file of the transaction "
                      + transaction_id + " does not exist"};

    if (!lth_file::read(metadata_file, metadata_txt))
        throw Error { "failed to read metadata file of the transaction "
                      + transaction_id };

    try {
        lth_jc::JsonContainer metadata { metadata_txt };

        if (!ActionResponse::isValidActionMetadata(metadata)) {
            LOG_DEBUG("The file '%1%' contains invalid action metadata:\n%2%",
                      metadata_file, metadata.toString());
            throw Error  { "invalid action metadata of the transaction "
                           + transaction_id };
        }

        return metadata;
    } catch (const lth_jc::data_parse_error& e) {
        LOG_DEBUG("The metadata file '%1%' is not valid JSON: %2%",
                  metadata_file, e.what());
        throw Error { "invalid JSON in metadata file of the transaction "
                      + transaction_id };
    }
}

bool ResultsStorage::pidFileExists(const std::string& transaction_id)
{
    return fs::exists(spool_dir_path_ / transaction_id / PID);
}

static int readIntegerFromFile(const std::string& file_path)
{
    std::string number_txt {};

    if (!fs::exists(file_path) || !lth_file::read(file_path, number_txt))
        throw ResultsStorage::Error { "failed to read file " + file_path };

    try {
        return std::stoi(number_txt);
    } catch (const std::invalid_argument& e) {
        throw ResultsStorage::Error {
            "invalid value stored in file " + file_path
            + (number_txt.empty() ? "" : ": " + number_txt) };
    }
}

int ResultsStorage::getPID(const std::string& transaction_id)
{
    return readIntegerFromFile((spool_dir_path_ / transaction_id / PID).string());
}

bool ResultsStorage::outputIsReady(const std::string& transaction_id)
{
    return fs::exists(spool_dir_path_ / transaction_id / EXITCODE);
}

ActionOutput ResultsStorage::getOutput_(const std::string& transaction_id,
                                        bool get_exitcode)
{
    auto results_path = (spool_dir_path_ / transaction_id);

    ActionOutput output {};

    if (get_exitcode) {
        std::string exitcode_txt {};
        auto exitcode_file = (results_path / EXITCODE).string();
        output.exitcode = readIntegerFromFile(exitcode_file);
    }

    auto stderr_file = (results_path / STDERR).string();
    auto stdout_file = (results_path / STDOUT).string();

    if (fs::exists(stderr_file)) {
        if (!lth_file::read(stderr_file, output.std_err)) {
            LOG_ERROR("Failed to read error file '%1%'; this failure will be ignored",
                      stderr_file);
        } else {
            LOG_TRACE("Successfully read error file '%1%'", stderr_file);
        }
    }

    if (!fs::exists(stdout_file)) {
        LOG_DEBUG("Output file '%1%' does not exist", stdout_file);
    } else if (!lth_file::read(stdout_file, output.std_out)) {
        throw Error { "failed to read " + stdout_file };
    } else if (output.std_out.empty()) {
        LOG_TRACE("Output file '%1%' is empty", stdout_file);
    } else {
        LOG_TRACE("Successfully read output file '%1%'", stdout_file);
    }

    return output;
}

ActionOutput ResultsStorage::getOutput(const std::string& transaction_id)
{
    return getOutput_(transaction_id, true);
}

ActionOutput ResultsStorage::getOutput(const std::string& transaction_id,
                                       int exitcode)
{
    auto output = getOutput_(transaction_id, false);
    output.exitcode = exitcode;
    return output;
}

static void defaultPurgeCallback(const std::string& dir_path)
{
    fs::remove_all(dir_path);
}

unsigned int ResultsStorage::purge(
                const std::string& ttl,
                const std::vector<std::string>& ongoing_transactions,
                std::function<void(const std::string& dir_path)> purge_callback)
{
    int num_purged_dirs { 0 };
    Timestamp ts { ttl };
    if (purge_callback == nullptr)
        purge_callback = &defaultPurgeCallback;

    lth_file::each_subdirectory(
        spool_dir_path_.string(),
        [&](std::string const& s) -> bool {
            fs::path dir_path { s };
            auto transaction_id = dir_path.filename().string();

            if (!ongoing_transactions.empty()
                    && std::find(ongoing_transactions.begin(),
                                 ongoing_transactions.end(),
                                 transaction_id) != ongoing_transactions.end())
                return true;

            try {
                auto md = getActionMetadata(transaction_id);
                if (md.get<std::string>("status") != "running"
                        && ts.isNewerThan(md.get<std::string>("start"))) {
                    LOG_TRACE("Removing %1%", s);
                    try {
                        purge_callback(dir_path.string());
                        num_purged_dirs++;
                    } catch (const std::exception& e) {
                        LOG_ERROR("Failed to remove %1%: %2%", s, e.what());
                    }
                }
            } catch (const Error& e) {
                LOG_DEBUG("Failed to get metadata for transaction %1% "
                          "(the results directory will not be removed): %2%",
                          transaction_id, e.what());
            }

            return true;
        });

    return num_purged_dirs;
}

}  // namespace PXPAgent
