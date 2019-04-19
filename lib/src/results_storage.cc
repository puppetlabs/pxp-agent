#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/time.hpp>

#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.results_storage"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>  // std::find

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc   = leatherman::json_container;
namespace lth_file = leatherman::file_util;
namespace lth_loc  = leatherman::locale;

static const std::string METADATA { "metadata" };
static const std::string STDOUT { "stdout" };
static const std::string STDERR { "stderr" };
static const std::string EXITCODE { "exitcode" };
static const std::string PID { "pid" };

ResultsStorage::ResultsStorage(std::string spool_dir, std::string spool_dir_ttl)
        : Purgeable { std::move(spool_dir_ttl) }, spool_dir_path_ { std::move(spool_dir) }
{
}

bool ResultsStorage::find(const std::string& transaction_id)
{
    auto p = spool_dir_path_ / transaction_id;
    return fs::exists(p) && fs::is_directory(p);
}

static void writeMetadata(const lth_jc::JsonContainer& metadata, const std::string& file_path) {
    // Redact "request_params" key in case parameters are sensitive.
    lth_jc::JsonContainer metadata_ { metadata };
    metadata_.set<std::string>("request_params", "{}");
    std::string txt = metadata_.toString() + "\n";
    try {
        lth_file::atomic_write_to_file(txt, file_path, NIX_FILE_PERMS, std::ios::binary);
    } catch (const std::exception& e) {
        throw ResultsStorage::Error {
            lth_loc::format("failed to write metadata: {1}", e.what()) };
    }
}

void ResultsStorage::initializeMetadataFile(const std::string& transaction_id,
                                            const lth_jc::JsonContainer& metadata)
{
    auto results_path = spool_dir_path_ / transaction_id;

    if (!fs::exists(results_path)) {
        LOG_DEBUG("Creating results directory for the  transaction {1} in '{2}'",
                  transaction_id, results_path.string());
        try {
            fs::create_directories(results_path);
            fs::permissions(results_path, NIX_DIR_PERMS);
        } catch (const fs::filesystem_error& e) {
            throw ResultsStorage::Error {
                lth_loc::format("failed to create results directory '{1}'",
                                e.what()) };
        }
    }

    auto metadata_file = (results_path / METADATA).string();
    writeMetadata(metadata, metadata_file);
}

void ResultsStorage::updateMetadataFile(const std::string& transaction_id,
                                        const lth_jc::JsonContainer& metadata)
{
    if (!find(transaction_id))
        throw Error {
            lth_loc::format("no results directory for the transaction {1}",
                            transaction_id) };

    auto metadata_file = (spool_dir_path_ / transaction_id / METADATA).string();
    writeMetadata(metadata, metadata_file);
}

lth_jc::JsonContainer
ResultsStorage::getActionMetadata(const std::string& transaction_id)
{
    auto metadata_file = (spool_dir_path_ / transaction_id / METADATA).string();
    std::string metadata_txt {};

    if (!fs::exists(metadata_file))
        throw Error {
            lth_loc::format("metadata file of the transaction {1} does not exist",
                            transaction_id) };

    if (!lth_file::read(metadata_file, metadata_txt))
        throw Error {
            lth_loc::format("failed to read metadata file of the transaction {1}",
                            transaction_id) };

    try {
        lth_jc::JsonContainer metadata { metadata_txt };

        if (!ActionResponse::isValidActionMetadata(metadata)) {
            LOG_DEBUG("The file '{1}' contains invalid action metadata:\n{2}",
                      metadata_file, metadata.toString());
            throw Error  {
                lth_loc::format("invalid action metadata of the transaction {1}",
                                transaction_id) };
        }

        return metadata;
    } catch (const lth_jc::data_parse_error& e) {
        LOG_DEBUG("The metadata file '{1}' is not valid JSON: {2}",
                  metadata_file, e.what());
        throw Error {
            lth_loc::format("invalid JSON in metadata file of the transaction {1}",
                            transaction_id) };
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
        throw ResultsStorage::Error {
            lth_loc::format("failed to read file '{1}'", file_path) };

    try {
        return std::stoi(number_txt);
    } catch (const std::invalid_argument& e) {
        throw ResultsStorage::Error {
            lth_loc::format("invalid value stored in file '{1}'{2}",
                            file_path,
                            (number_txt.empty() ? "" : ": " + number_txt)) };
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
            LOG_ERROR("Failed to read error file '{1}'; this failure will be ignored",
                      stderr_file);
        } else {
            LOG_TRACE("Successfully read error file '{1}'", stderr_file);
        }
    }

    if (!fs::exists(stdout_file)) {
        LOG_DEBUG("Output file '{1}' does not exist", stdout_file);
    } else if (!lth_file::read(stdout_file, output.std_out)) {
        throw Error { lth_loc::format("failed to read '{1}'", stdout_file) };
    } else if (output.std_out.empty()) {
        LOG_TRACE("Output file '{1}' is empty", stdout_file);
    } else {
        LOG_TRACE("Successfully read output file '{1}'", stdout_file);
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

unsigned int ResultsStorage::purge(
                const std::string& ttl,
                std::vector<std::string> ongoing_transactions,
                std::function<void(const std::string& dir_path)> purge_callback)
{
    unsigned int num_purged_dirs { 0 };
    Timestamp ts { ttl };
    if (purge_callback == nullptr)
        purge_callback = &Purgeable::defaultDirPurgeCallback;

    LOG_INFO("About to purge the results directories from '{1}'; TTL = {2}",
             spool_dir_path_.string(), ttl);

    lth_file::each_subdirectory(
        spool_dir_path_.string(),
        [&](std::string const& s) -> bool {
            fs::path dir_path { s };
            auto transaction_id = dir_path.filename().string();
            LOG_TRACE("Inspecting '{1}' for purging", s);

            if (!ongoing_transactions.empty()
                    && std::find(ongoing_transactions.begin(),
                                 ongoing_transactions.end(),
                                 transaction_id) != ongoing_transactions.end())
                return true;

            try {
                auto md = getActionMetadata(transaction_id);

                if (md.get<std::string>("status") == "running") {
                    LOG_TRACE("Skipping '{1}' as the action status is 'running'", s);
                } else if (ts.isNewerThan(md.get<std::string>("start"))) {
                    LOG_TRACE("Removing '{1}'", s);

                    try {
                        purge_callback(dir_path.string());
                        num_purged_dirs++;
                    } catch (const std::exception& e) {
                        LOG_ERROR("Failed to remove '{1}': {2}", s, e.what());
                    }
                }
            } catch (const Error& e) {
                LOG_WARNING("Failed to retrieve the metadata for the transaction {1} "
                            "(the results directory will not be removed): {2}",
                            transaction_id, e.what());
            } catch (const Timestamp::Error& e) {
                LOG_WARNING("Failed to process the metadata for the transaction {1} "
                            "(the results directory will not be removed): {2}",
                            transaction_id, e.what());
            }

            return true;
        });

    LOG_INFO(lth_loc::format_n(
        // LOCALE: info
        "Removed {1} directory from '{2}'",
        "Removed {1} directories from '{2}'",
        num_purged_dirs, num_purged_dirs, spool_dir_path_.string()));
    return num_purged_dirs;
}

}  // namespace PXPAgent
