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

using namespace PXPAgent;

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR, "0d");

static const std::string TASK_CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/tasks-cache" };

// Disable cache ttl so we don't delete fixtures.
static const std::string TASK_CACHE_TTL { "0d" };

static const auto MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(TASK_CACHE_DIR, TASK_CACHE_TTL);

static const std::string TEMP_TASK_CACHE_DIR { TASK_CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

static const std::string CRL { "mock_crl" };

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
        REQUIRE_NOTHROW(Modules::Task(PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE));
    }
}

TEST_CASE("Modules::Task::features", "[modules]") {
    Modules::Task mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };

    SECTION("reports available features") {
        REQUIRE(mod.features().size() > 1);
        REQUIRE(mod.features().find("puppet-agent") != mod.features().end());
#ifdef _WIN32
        REQUIRE(mod.features().find("powershell") != mod.features().end());
#else
        REQUIRE(mod.features().find("shell") != mod.features().end());
#endif
    }
}

TEST_CASE("Modules::Task::hasAction", "[modules]") {
    Modules::Task mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };

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
    static const auto temp_module_cache_dir = std::make_shared<ModuleCacheDir>(TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL);
    Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, temp_module_cache_dir, STORAGE };

    SECTION("throws module processing error when a file system error is thrown") {
        auto existent_sha = "existent";
        boost::nowide::ofstream(TEMP_TASK_CACHE_DIR + "/" + existent_sha);
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"test::existent\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"existent\"}]}").str();
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
        REQUIRE(response.action_metadata.get<std::string>("execution_error").find("Failed to create cache dir to download file to"));
    }
}

TEST_CASE("Modules::Task::callAction - non blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };
    Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };

    SECTION("the pid is written to file") {
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
    Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };

    SECTION("passes input as json") {
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"15f26bdeea9186293d256db95fed616a7b823de947f4e9bd0d8d23c5ac786d13\", \"filename\": \"init\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"e1c10f8c709f06f4327ac6a07a918e297a039a24a788fabf4e2ebc31d16e8dc3\", \"filename\": \"init.bat\"}]}").str();
#endif
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({"results", "stdout"});
        boost::trim(output);
        REQUIRE(output == "{\"message\":\"hello\",\"_task\":\"test\"}");
    }

    SECTION("builds installdir") {
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test\","
                           "\"input_method\": \"environment\","
                           "\"input\":{\"message\":\"hello\"},"
                           "\"metadata\": {\"files\": [\"dir/\"],"
                                          "\"implementations\": [{\"name\": \"init.sh\", \"requirements\": [\"shell\"], \"files\": [\"file1.txt\"]}]},"
                           "\"files\" : [{\"sha256\": \"28dd63928f9e3837e11e36f5af35c09068e4d62b355cf169a873a0cdf30f7c95\", \"filename\": \"init.sh\"},"
                                        "{\"sha256\": \"67ee5478eaadb034ba59944eb977797b49ca6aa8d3574587f36ebcbeeb65f70e\", \"filename\": \"dir/file2.txt\"},"
                                        "{\"sha256\": \"94f6e58bd04a4513b8301e75f40527cf7610c66d1960b26f6ac2e743e108bdac\", \"filename\": \"dir/sub_dir/file3.txt\"},"
                                        "{\"sha256\": \"ecdc5536f73bdae8816f0ea40726ef5e9b810d914493075903bb90623d97b1d8\", \"filename\": \"file1.txt\"}]}").str();
        auto task_path = fs::path(TASK_CACHE_DIR) / "28dd63928f9e3837e11e36f5af35c09068e4d62b355cf169a873a0cdf30f7c95" / "init.sh";
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test\","
                           "\"input_method\": \"environment\","
                           "\"input\":{\"message\":\"hello\"},"
                           "\"metadata\": {\"files\": [\"dir/\"],"
                                          "\"implementations\": [{\"name\": \"init.bat\", \"requirements\": [], \"files\": [\"file1.txt\"]}]},"
                           "\"files\" : [{\"sha256\": \"ea7d652bfe797121a7ac8e3654aacf7d50a9a4665e843669b873995b072d820b\", \"filename\": \"init.bat\"},"
                                        "{\"sha256\": \"67ee5478eaadb034ba59944eb977797b49ca6aa8d3574587f36ebcbeeb65f70e\", \"filename\": \"dir/file2.txt\"},"
                                        "{\"sha256\": \"94f6e58bd04a4513b8301e75f40527cf7610c66d1960b26f6ac2e743e108bdac\", \"filename\": \"dir/sub_dir/file3.txt\"},"
                                        "{\"sha256\": \"ecdc5536f73bdae8816f0ea40726ef5e9b810d914493075903bb90623d97b1d8\", \"filename\": \"file1.txt\"}]}").str();
        auto task_path = fs::path(TASK_CACHE_DIR) / "ea7d652bfe797121a7ac8e3654aacf7d50a9a4665e843669b873995b072d820b" / "init.bat";
#endif
        std::ifstream task_file { task_path.string() };
        std::string task_content { std::istreambuf_iterator<char>{task_file}, {} };
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({"results", "stdout"});
        boost::trim(output);
        boost::trim(task_content);
        REQUIRE(output == "file1\nfile2\nfile3\n"+task_content);
    }

    SECTION("passes input only on stdin when input_method is stdin") {
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test::multi\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"stdin\", \"files\" : [{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test::multi\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"stdin\", \"files\" : [{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}]}").str();
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
        REQUIRE(output == "ECHO is on.\r\n{\"message\":\"hello\",\"_task\":\"test::multi\"}");
#else
        REQUIRE(output == "{\"message\":\"hello\",\"_task\":\"test::multi\"}");
#endif
    }

    SECTION("passes input on both stdin and env when method is both") {
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::both\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"both\", \"files\" : [{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::both\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"both\", \"files\" : [{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}]}").str();
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
        REQUIRE(output == "hello\r\n{\"message\":\"hello\",\"_task\":\"test::both\"}");
#else
        REQUIRE(output == "hello\n{\"message\":\"hello\",\"_task\":\"test::both\"}");
#endif
    }

    SECTION("passes input as env variables") {
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::printer\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"936e85a9b7f1e7b4b593c9f051a36105ed36f7fb8dcff67ff23a3a9af2abe962\", \"filename\": \"printer\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::printer\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\": \"1c616ed98f54880444d0c49036cdf930120457c20e7a9a204db750f2d6162999\", \"filename\": \"printer.bat\"}]}").str();
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
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::multi\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"environment\", \"files\" : [{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\":\"test::multi\", \"input\":{\"message\":\"hello\"}, \"input_method\": \"environment\", \"files\" : [{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}]}").str();
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
        auto echo_txt =
#ifndef _WIN32
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test::error\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\" : \"d5b8819b51ecd53b32de74c09def0e71f617076bc8e4f75e1eac99b8f77a6c70\", \"filename\": \"error\"}]}").str();
#else
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"test::error\", \"input\":{\"message\":\"hello\"}, \"files\" : [{\"sha256\" : \"554f86a33add88c371c2bbb79839c9adfd3d420dc5f405a07e97fab54efbe1ba\", \"filename\": \"error.bat\"}]}").str();
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
        REQUIRE(response.action_metadata.get<std::string>("execution_error") == "The task executed for the blocking 'task run' request (transaction 0632) returned invalid UTF-8 on stdout");
        boost::trim(response.output.std_out);
        unsigned char badchar[3] = {0xed, 0xbf, 0xbf};
        REQUIRE(response.output.std_out == std::string(reinterpret_cast<char*>(badchar), 3));
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }

// Unclear how to create a script on Windows that can print a null byte...
#ifndef _WIN32
    SECTION("errors on output with null bytes") {
        auto echo_txt =
            (DATA_FORMAT % "\"0632\""
                         % "\"task\""
                         % "\"run\""
                         % "{\"task\": \"null_byte\", \"input\":{\"message\":\"hello\"}, "
                           "\"files\" : [{\"sha256\": \"b26e34bc50c88ca5ee2bfcbcaff5c23b0124db9479e66390539f2715b675b7e7\", \"filename\": \"null_byte\"}]}").str();
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
        REQUIRE(response.action_metadata.get<std::string>("execution_error") == "The task executed for the blocking 'task run' request (transaction 0632) returned invalid UTF-8 on stdout");
        boost::trim(response.output.std_out);
        unsigned char badchar[7] = {'f', 'o', 'o', 0x00, 'b', 'a', 'r'};
        REQUIRE(response.output.std_out == std::string(reinterpret_cast<char*>(badchar), 7));
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }
#endif

    SECTION("errors on download when no master-uri is provided") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, {}, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
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
        REQUIRE(boost::contains(response.action_metadata.get<std::string>("execution_error"), "Cannot download file. No master-uris were provided"));
    }

    SECTION("if a master-uri has a server-side error, then it proceeds to try the next master-uri. if they all fail, it errors on download and removes all temporary files") {
        static const auto temp_module_cache_dir = std::make_shared<ModuleCacheDir>(TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL);
        Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, temp_module_cache_dir, STORAGE };
        auto cache = fs::path(TEMP_TASK_CACHE_DIR) / "some_sha";
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
        static const auto temp_module_cache_dir = std::make_shared<ModuleCacheDir>(TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL);
        Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, temp_module_cache_dir, STORAGE };
        auto cache = fs::path(TEMP_TASK_CACHE_DIR) / "some_other_sha";
        auto task_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"sha256\": \"some_other_sha\",  \"filename\": \"some_file\"}]}").str();
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
        static const auto temp_module_cache_dir = std::make_shared<ModuleCacheDir>(TEMP_TASK_CACHE_DIR, TASK_CACHE_TTL);
        Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, temp_module_cache_dir, STORAGE };
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

template <typename T>
static ActionRequest getEchoRequest(T& metadata)
{
    auto files = "["
        "{\"sha256\": \"823c013467ce03b12dbe005757a6c842894373e8bcfb0cf879329afb5abcd543\", \"filename\": \"multi\"},"
        "{\"sha256\": \"88a07e5b672aa44a91aa7d63e22c91510af5d4707e12f75e0d5de2dfdbde1dec\", \"filename\": \"multi.bat\"}"
        "]";
    auto params = boost::format("{\"task\": \"test::multi\", \"metadata\": %1%, \"input\":{\"message\":\"hello\"}, \"files\": %2%}") % metadata % files;
    auto echo_txt = (DATA_FORMAT % "\"0632\"" % "\"task\"" % "\"run\"" % params).str();
    PCPClient::ParsedChunks echo_content {
        lth_jc::JsonContainer(ENVELOPE_TXT),
        lth_jc::JsonContainer(echo_txt),
        {},
        0 };
    return ActionRequest { RequestType::Blocking, echo_content };
}

TEST_CASE("Modules::Task::executeAction implementations", "[modules][output]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };
    Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, MODULE_CACHE_DIR, STORAGE };

    SECTION("uses a matching implementation") {
        auto impls = "["
            "{\"name\": \"multi\", \"requirements\": [\"shell\"]},"
            "{\"name\": \"multi.bat\", \"requirements\": [\"powershell\"]},"
            "{\"name\": \"invalid\"}"
            "]";
        auto metadata = boost::format("{\"implementations\": %1%, \"input_method\": \"environment\", \"features\": [\"foobar\"]}") % impls;

        auto output = e_m.executeAction(getEchoRequest(metadata)).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("uses an unrestricted implementation") {
        auto impls = "["
            "{\"name\": \"invalid\", \"requirements\": [\"foobar\"]},"
#ifdef _WIN32
            "{\"name\": \"multi.bat\"}"
#else
            "{\"name\": \"multi\"}"
#endif
            "]";
        auto metadata = boost::format("{\"implementations\": %1%, \"input_method\": \"environment\"}") % impls;

        auto output = e_m.executeAction(getEchoRequest(metadata)).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("accepts features as an argument") {
        auto impls = "["
#ifdef _WIN32
            "{\"name\": \"multi.bat\", \"requirements\": [\"foobar\"]}"
#else
            "{\"name\": \"multi\", \"requirements\": [\"foobar\"]}"
#endif
            "]";
        auto metadata = boost::format("{\"implementations\": %1%, \"input_method\": \"environment\", \"features\": [\"foobar\"]}") % impls;

        auto output = e_m.executeAction(getEchoRequest(metadata)).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("uses the implementation's input_method") {
        auto impls = "["
#ifdef _WIN32
            "{\"name\": \"multi.bat\", \"input_method\": \"environment\"}"
#else
            "{\"name\": \"multi\", \"input_method\": \"environment\"}"
#endif
            "]";
        auto metadata = boost::format("{\"implementations\": %1%}") % impls;

        auto output = e_m.executeAction(getEchoRequest(metadata)).action_metadata.get<std::string>({ "results", "stdout" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("errors if no implementations are accepted") {
        auto impls = "[{\"name\": \"invalid\", \"requirements\": [\"foobar\"]}]";
        auto metadata = boost::format("{\"implementations\": %1%, \"input_method\": \"environment\"}") % impls;

        auto response = e_m.executeAction(getEchoRequest(metadata));
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE(boost::contains(response.action_metadata.get<std::string>("execution_error"),
                                "no implementations match supported features: "));
    }

    SECTION("errors if implementation specifies file that doesn't exist") {
        auto impls = "[{\"name\": \"invalid\"}]";
        auto metadata = boost::format("{\"implementations\": %1%, \"input_method\": \"environment\"}") % impls;

        auto response = e_m.executeAction(getEchoRequest(metadata));
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE(boost::contains(response.action_metadata.get<std::string>("execution_error"),
                                "'invalid' file requested by implementation not found"));
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

    static const auto PURGE_MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(PURGE_TASK_CACHE, TASK_CACHE_TTL);

    // Start with 0 TTL to prevent initial cleanup
    Modules::Task e_m { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", 10, 20, PURGE_MODULE_CACHE_DIR, STORAGE };

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

        auto results = e_m.purge("1h", {}, purgeCallback);
        REQUIRE(results == 1);
        REQUIRE(num_purged_results == 1);
    }
}
