#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/process.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <unistd.h>

#ifdef _WIN32
#define EXTENSION ".bat"
#else
#define EXTENSION ""
#endif

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR);

static const std::string TASK_CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/tasks-cache" };

// Disable cache ttl so we don't delete fixtures.
static const std::string TASK_CACHE_TTL { "0d" };

static const std::string TEMP_TASK_CACHE_DIR { TASK_CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

static const std::string NON_BLOCKING_ECHO_TXT {
    (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                              % "\"task\""
                              % "\"run\""
                              % "{\"task\":\"foo\",\"input\":{\"message\":\"hello\"},"
                                 "\"files\":[{\"uri\":{\"path\":\"/init" EXTENSION "\","
                                                      "\"params\":{\"environment\":\"production\"}},"
                                                      "\"sha256\":\"15f26bdeea9186293d256db95fed616a7b823de947f4e9bd0d8d23c5ac786d13\","
                                                      "\"filename\":\"init\","
                                                      "\"size_bytes\":131}]}"
                              % "false").str() };

static const PCPClient::ParsedChunks NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(NON_BLOCKING_ECHO_TXT),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

TEST_CASE("Modules::Task", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Task(PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE));
    }
}

TEST_CASE("Modules::Task::hasAction", "[modules]") {
    Modules::Task mod { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };

    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("run"));
    }
}

static void configureTest() {
    if (!fs::exists(SPOOL_DIR) && !fs::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }
    if (!fs::exists(TEMP_TASK_CACHE_DIR) && !fs::create_directories(TEMP_TASK_CACHE_DIR)) {
        FAIL("Failed to create the temporary tasks cache directory");
    }
    Configuration::Instance().initialize(
        [](std::vector<std::string>) {
            return EXIT_SUCCESS;
        });
}

static void resetTest() {
    if (fs::exists(SPOOL_DIR)) {
        fs::remove_all(SPOOL_DIR);
    }
    if (fs::exists(TEMP_TASK_CACHE_DIR)) {
        fs::remove_all(TEMP_TASK_CACHE_DIR);
    }
}

TEST_CASE("Modules::Task::callAction", "[modules]") {
  configureTest();
  lth_util::scope_exit config_cleaner { resetTest };

  SECTION("throws module processing error when a file system error is thrown") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto existent_sha = "existent";
        boost::nowide::ofstream(TEMP_TASK_CACHE_DIR + "/" + existent_sha);
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"existent\"}]}").str();
        PCPClient::ParsedChunks task_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(task_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, task_content };

        auto response = e_m.executeAction(request);
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE_THAT(response.action_metadata.get<std::string>("execution_error"), Catch::EndsWith("A file matching the name of the provided sha already exists"));
  }
}

TEST_CASE("Modules::Task::callAction - non blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("the pid is written to file") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        ActionRequest request { RequestType::NonBlocking, NON_BLOCKING_CONTENT };
        fs::path spool_path { SPOOL_DIR };
        auto results_dir = (spool_path / request.transactionId()).string();
        fs::create_directories(results_dir);
        request.setResultsDir(results_dir);
        auto pid_path = spool_path / request.transactionId() / "pid";

        REQUIRE_NOTHROW(e_m.executeAction(request));
        REQUIRE(fs::exists(pid_path));

        try {
            auto pid_txt = lth_file::read(pid_path.string());
            auto pid = std::stoi(pid_txt);
        } catch (std::exception&) {
            FAIL("fail to get pid");
        }
    }
}

TEST_CASE("Modules::Task::executeAction", "[modules][output]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("passes input as json") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"15f26bdeea9186293d256db95fed616a7b823de947f4e9bd0d8d23c5ac786d13\", \"filename\": \"init\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"e1c10f8c709f06f4327ac6a07a918e297a039a24a788fabf4e2ebc31d16e8dc3\", \"filename\": \"init.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({"results", "stdout"});
        boost::trim(output);
        REQUIRE(output == "{\"message\":\"hello\"}");
    }

    SECTION("passes input only on stdin when input_method is stdin") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"input_method\": \"stdin\", \"files\" : [{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"input_method\": \"stdin\", \"files\" : [{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
#ifdef _WIN32
        REQUIRE(output == "ECHO is on.\r\n{\"message\":\"hello\"}");
#else
        REQUIRE(output == "{\"message\":\"hello\"}");
#endif
    }

    SECTION("passes input as env variables") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"936e85a9b7f1e7b4b593c9f051a36105ed36f7fb8dcff67ff23a3a9af2abe962\", \"filename\": \"printer\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"1c616ed98f54880444d0c49036cdf930120457c20e7a9a204db750f2d6162999\", \"filename\": \"printer.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({"results", "stdout"});
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("passes input only as env variables when input_method is environment") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"input_method\": \"environment\", \"files\" : [{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"input_method\": \"environment\", \"files\" : [{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("succeeds on non-zero exit") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\" : \"d5b8819b51ecd53b32de74c09def0e71f617076bc8e4f75e1eac99b8f77a6c70\", \"filename\": \"error\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\" : \"554f86a33add88c371c2bbb79839c9adfd3d420dc5f405a07e97fab54efbe1ba\", \"filename\": \"error.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };
        auto response = e_m.executeAction(request);

        auto output = response.action_metadata.get<std::string>({"results", "stdout"});
        boost::trim(output);
        REQUIRE(output == "hello");
        REQUIRE(response.action_metadata.get<bool>("results_are_valid"));
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "hello");
        boost::trim(response.output.std_err);
        REQUIRE(response.output.std_err == "goodbye");
        REQUIRE(response.output.exitcode == 1);
    }

    SECTION("errors on unparseable output") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                           "\"files\" : [{\"sha256\": \"d2795e0a1b66ca75be9e2be25c2a61fdbab9efc641f8e480f5ab1b348112701d\", \"filename\": \"unparseable\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                           "\"files\" : [{\"sha256\": \"0d75633b5dd4b153496b4593e9d94e69265d2a812579f724ba0b4422b0bfb836\", \"filename\": \"unparseable.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };
        auto response = e_m.executeAction(request);

        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE(response.action_metadata.get<std::string>("execution_error") == "The task executed for the blocking 'task run' request (transaction 0632) returned invalid UTF-8 on stdout - stderr: (empty)");
        boost::trim(response.output.std_out);
        unsigned char badchar[3] = {0xed, 0xbf, 0xbf};
        REQUIRE(response.output.std_out == std::string(reinterpret_cast<char*>(badchar), 3));
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }

    SECTION("errors on download when no master-uri is provided") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL, {}, CA, CRT, KEY, STORAGE };
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"sha256\": \"some_sha\", \"filename\": \"some_file\"}]}").str();
        PCPClient::ParsedChunks task_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(task_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, task_content };
        auto response = e_m.executeAction(request);

        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE(boost::contains(response.action_metadata.get<std::string>("execution_error"), "Cannot download task. No master-uris were provided"));
    }

    SECTION("if a master-uri has a server-side error, then it proceeds to try the next master-uri. if they all fail, it errors on download and removes all temporary files") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto cache = fs::path(TEMP_TASK_CACHE_DIR) / "some_sha";
        std::string bad_path = "bad_path";
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"uri\": { \"path\": \"bad_path\", \"params\": { \"environment\": \"production\", \"code-id\": \"1234\" }}, \"sha256\": \"some_sha\", \"filename\": \"some_file\"}]}").str();
        PCPClient::ParsedChunks task_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(task_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, task_content };
        auto response = e_m.executeAction(request);

        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        std::string expected_master_uri = MASTER_URIS.back();
        boost::erase_head(expected_master_uri, std::string("https://").length());
        REQUIRE_THAT(response.action_metadata.get<std::string>("execution_error"), Catch::Contains(expected_master_uri));
        // Make sure temp file is removed.
        REQUIRE(fs::is_empty(cache));
    }

    SECTION("creates the tasks-cache/<sha> directory with ower/group read and write permissions") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        auto cache = fs::path(TEMP_TASK_CACHE_DIR) / "some_other_sha";
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"sha256\": \"some_other_sha\"}]}").str();
        PCPClient::ParsedChunks task_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(task_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, task_content };
        e_m.executeAction(request);

        REQUIRE(fs::exists(cache));
        REQUIRE(fs::is_directory(cache));
#ifndef _WIN32
        REQUIRE(fs::status(cache).permissions() == 0750);
#else
        REQUIRE(fs::status(cache).permissions() == 0666);
#endif
    }

    SECTION("succeeds even if tasks-cache was deleted") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };
        fs::remove_all(TEMP_TASK_CACHE_DIR);

        auto cache = fs::path(TEMP_TASK_CACHE_DIR) / "some_other_sha";
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"sha256\": \"some_other_sha\"}]}").str();
        PCPClient::ParsedChunks task_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(task_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, task_content };
        e_m.executeAction(request);

        REQUIRE(fs::exists(cache));
        REQUIRE(fs::is_directory(cache));
    }
}

// Present in Boost 1.58. That's currently not required, so reproducing it here since it's simple.
static std::time_t my_to_time_t(pt::ptime t)
{
    return (t - pt::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();
}

TEST_CASE("purge old tasks", "[modules]") {
    const std::string PURGE_TASK_CACHE { std::string { PXP_AGENT_ROOT_PATH }
        + "/lib/tests/resources/purge_test" };

    const std::string OLD_TRANSACTION { "valid_old" };
    const std::string RECENT_TRANSACTION { "valid_recent" };

    // Start with 0 TTL to prevent initial cleanup
    Modules::Task e_m { PXP_AGENT_BIN_PATH, PURGE_TASK_CACHE, TASK_CACHE_TTL, MASTER_URIS, CA, CRT, KEY, STORAGE };

    unsigned int num_purged_results { 0 };
    auto purgeCallback =
        [&num_purged_results](const std::string&) -> void { num_purged_results++; };

    SECTION("Purges only the old results based on ttl") {
        num_purged_results = 0;
        auto now = pt::second_clock::universal_time();
        auto recent = now - pt::minutes(50);
        auto old = now - pt::minutes(61);
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/OLD_TRANSACTION, my_to_time_t(old));
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/RECENT_TRANSACTION, my_to_time_t(recent));

        auto results = e_m.purge("1h", purgeCallback);
        REQUIRE(results == 1);
        REQUIRE(num_purged_results == 1);
    }
}

}  // namespace PXPAgent
