#include "root_path.hpp"
#include "../content_format.hpp"                    // ENVELOPE_TXT

#include <pxp-agent/modules/status.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/util/strings.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>

#include <catch.hpp>

#include <cstdio>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace HW = HorseWhisperer;

const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                      + "/lib/tests/resources/test_spool/" };

static const std::string QUERY_ACTION { "query" };

boost::format STATUS_FORMAT {
    "{  \"transaction_id\" : \"2345236346\","
    "    \"module\" : \"status\","
    "    \"action\" : \"query\","
    "    \"params\" : {\"transaction_id\" : \"%1%\"}"
    "}"
};

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};

TEST_CASE("Modules::Status::executeAction", "[modules]") {
    Modules::Status status_module {};

    SECTION("the status module is correctly named") {
        REQUIRE(status_module.module_name == "status");
    }

    SECTION("the status module has the 'query' action") {
        auto found = std::find(status_module.actions.begin(),
                               status_module.actions.end(),
                               QUERY_ACTION);
        REQUIRE(found != status_module.actions.end());
    }

    SECTION("it can call the 'query' action") {
        PCPClient::ParsedChunks PARSED_CHUNKS {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer((STATUS_FORMAT % "the-uuid-string").str()),
            NO_DEBUG,
            0 };
        ActionRequest request { RequestType::Blocking, PARSED_CHUNKS };
        REQUIRE_NOTHROW(status_module.executeAction(request));
    }

    SECTION("it works properly when an unknown job id is provided") {
        auto job_id = lth_util::get_UUID();
        std::string other_status_txt { (STATUS_FORMAT % job_id).str() };
        PCPClient::ParsedChunks other_chunks {
                lth_jc::JsonContainer(ENVELOPE_TXT),
                lth_jc::JsonContainer(other_status_txt),
                NO_DEBUG,
                0 };
        ActionRequest request { RequestType::Blocking, other_chunks };

        SECTION("it doesn't throw") {
            REQUIRE_NOTHROW(status_module.executeAction(request));
        }

        SECTION("it returns status 'unknown'") {
            auto outcome = status_module.executeAction(request);
            REQUIRE(outcome.results.get<std::string>("status") == "unknown");
        }
    }

    if (!boost::filesystem::exists(SPOOL_DIR)
        && !boost::filesystem::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }

    auto testResultFiles =
        [&] (bool success, std::string result_path) {
            HW::SetFlag<std::string>("spool-dir", SPOOL_DIR);
            auto symlink_name = lth_util::get_UUID();
            std::string other_status_txt { (STATUS_FORMAT % symlink_name).str() };
            PCPClient::ParsedChunks other_chunks {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(other_status_txt),
                    NO_DEBUG,
                    0 };
            ActionRequest request { RequestType::Blocking, other_chunks };

            boost::filesystem::path dest { SPOOL_DIR };
            dest /= symlink_name;
            if (!boost::filesystem::exists(dest)
                    && !boost::filesystem::create_directories(dest)) {
                FAIL("Failed to create test directory");
            }

            try {
                std::for_each(boost::filesystem::directory_iterator(result_path),
                              boost::filesystem::directory_iterator(),
                              [&](boost::filesystem::directory_entry &d) {
                                  boost::filesystem::copy(
                                    d.path(), dest / d.path().filename());
                              });
            } catch (boost::filesystem::filesystem_error &e) {
                FAIL((boost::format("Failed to copy files: %1%") % e.what()).str());
            }

            SECTION("it doesn't throw") {
                REQUIRE_NOTHROW(status_module.executeAction(request));
            }

            SECTION("it returns the action status") {
                auto outcome = status_module.executeAction(request);
                std::string status { (success ? "success" : "failure") };
                REQUIRE(outcome.results.get<std::string>("status") == status);
            }

            SECTION("it returns the action exitcode") {
                auto outcome = status_module.executeAction(request);
                int exitcode { (success ? 0 : 4) };
                REQUIRE(outcome.results.get<int>("exitcode") == exitcode);
            }

            SECTION("it returns the action output") {
                // On Windows the line-ending of the test file will
                // depend on your git configuration, so allow for
                // \r\n or just \n.
                auto outcome = status_module.executeAction(request);
                boost::regex out { (success ? "\\*\\*\\*OUTPUT\\r?\\n" : "") };
                REQUIRE(boost::regex_match(
                    outcome.results.get<std::string>("stdout"), out));
            }

            SECTION("it returns the action error string") {
                auto outcome = status_module.executeAction(request);
                boost::regex err { (success ? "" : "\\*\\*\\*ERROR\\r?\\n") };
                REQUIRE(boost::regex_match(
                    outcome.results.get<std::string>("stderr"), err));
            }

            boost::filesystem::remove_all(dest);
        };

    SECTION("it correctly retrieves the results of a known job") {
        std::string result_path { PXP_AGENT_ROOT_PATH };

        SECTION("success") {
            result_path += "/lib/tests/resources/delayed_result_success";
            testResultFiles(true, result_path);
        }

        SECTION("failure") {
            result_path += "/lib/tests/resources/delayed_result_failure";
            testResultFiles(false, result_path);
        }
    }

    if (!boost::filesystem::exists(SPOOL_DIR)) {
        boost::filesystem::remove_all(SPOOL_DIR);
    }
}

}  // namespace PXPAgent
