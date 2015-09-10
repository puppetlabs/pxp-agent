#include "root_path.hpp"
#include "../content_format.hpp"                    // ENVELOPE_TXT

#include <pxp-agent/modules/status.hpp>
#include <pxp-agent/configuration.hpp>              // DEFAULT_ACTION_RESULTS_DIR

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/util/strings.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <catch.hpp>

#include <cstdio>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;

static const std::string QUERY_ACTION { "query" };

boost::format STATUS_FORMAT {
    "{  \"transaction_id\" : \"2345236346\","
    "    \"module\" : \"status\","
    "    \"action\" : \"query\","
    "    \"params\" : {\"transaction_id\" : \"%1%\"}"
    "}"
};

static const std::string STATUS_TXT { (STATUS_FORMAT % "the-uuid-string").str() };

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};

static const PCPClient::ParsedChunks PARSED_CHUNKS {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(STATUS_TXT),
                    NO_DEBUG,
                    0 };

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

        SECTION("it returns status 'Unknown'") {
            auto outcome = status_module.executeAction(request);
            REQUIRE(outcome.results.get<std::string>("status") == "Unknown");
        }
    }

    auto testResultFiles =
        [&] (bool success, std::string result_path) {
            boost::filesystem::path to { result_path };

            if (!boost::filesystem::exists(DEFAULT_ACTION_RESULTS_DIR)
                && !boost::filesystem::create_directory(DEFAULT_ACTION_RESULTS_DIR)) {
                FAIL("Failed to create the results directory");
            }

            auto symlink_name = lth_util::get_UUID();
            std::string symlink_path { DEFAULT_ACTION_RESULTS_DIR + symlink_name };
            boost::filesystem::path symlink { symlink_path };

            std::string other_status_txt { (STATUS_FORMAT % symlink_name).str() };
            PCPClient::ParsedChunks other_chunks {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(other_status_txt),
                    NO_DEBUG,
                    0 };
            ActionRequest request { RequestType::Blocking, other_chunks };

            try {
                boost::filesystem::create_symlink(to, symlink);
            } catch (boost::filesystem::filesystem_error) {
                FAIL("Failed to create the symlink");
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
                auto outcome = status_module.executeAction(request);
                std::string out { (success ? "***OUTPUT\n" : "") };
                REQUIRE(outcome.results.get<std::string>("stdout") == out);
            }

            SECTION("it returns the action error string") {
                auto outcome = status_module.executeAction(request);
                std::string err { (success ? "" : "***ERROR\n") };
                REQUIRE(outcome.results.get<std::string>("stderr") == err);
            }

            boost::filesystem::remove(symlink_path);
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
}

}  // namespace PXPAgent
