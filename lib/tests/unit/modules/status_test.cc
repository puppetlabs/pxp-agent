#include <cthun-agent/errors.hpp>
#include <cthun-agent/modules/status.hpp>
#include <cthun-agent/configuration.hpp>

#include "root_path.hpp"

#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/util/strings.hpp>

#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <catch.hpp>

#include <cstdio>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;

static const std::string query_action { "query" };

boost::format status_format {
    "{   \"module\" : \"status\","
    "    \"action\" : \"query\","
    "    \"params\" : {\"job_id\" : \"%1%\"}"
    "}"
};

static const std::string status_txt { (status_format % "the-uuid-string").str() };

static const std::vector<lth_jc::JsonContainer> no_debug {};

static const CthunClient::ParsedChunks parsed_chunks {
                    lth_jc::JsonContainer(),
                    lth_jc::JsonContainer(status_txt),
                    no_debug,
                    0 };

TEST_CASE("Modules::Status::executeAction", "[modules]") {
    Modules::Status status_module {};

    SECTION("the status module is correctly named") {
        REQUIRE(status_module.module_name == "status");
    }

    SECTION("the inventory module has the 'query' action") {
        auto found = std::find(status_module.actions.begin(),
                               status_module.actions.end(),
                               query_action);
        REQUIRE(found != status_module.actions.end());
    }

    SECTION("it can call the 'query' action") {
        ActionRequest request { RequestType::Blocking, parsed_chunks };
        REQUIRE_NOTHROW(status_module.executeAction(request));
    }

    SECTION("it works properly when an unknown job id is provided") {
        auto job_id = lth_util::get_UUID();
        std::string other_status_txt { (status_format % job_id).str() };
        CthunClient::ParsedChunks other_chunks {
                lth_jc::JsonContainer(),
                lth_jc::JsonContainer(other_status_txt),
                no_debug,
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

    SECTION("it correctly retrieves the file content of a known job") {
        std::string result_path { std::string { CTHUN_AGENT_ROOT_PATH }
                                  + "/lib/tests/resources/delayed_result" };
        boost::filesystem::path to { result_path };

        if (!boost::filesystem::exists(DEFAULT_ACTION_RESULTS_DIR)
            && !boost::filesystem::create_directory(DEFAULT_ACTION_RESULTS_DIR)) {
            FAIL("Failed to create the results directory");
        }

        auto symlink_name = lth_util::get_UUID();
        std::string symlink_path { DEFAULT_ACTION_RESULTS_DIR + symlink_name };
        boost::filesystem::path symlink { symlink_path };

        std::string other_status_txt { (status_format % symlink_name).str() };
        CthunClient::ParsedChunks other_chunks {
                lth_jc::JsonContainer(),
                lth_jc::JsonContainer(other_status_txt),
                no_debug,
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
            REQUIRE(outcome.results.get<std::string>("status") == "Completed");
        }

        SECTION("it returns the action output") {
            auto outcome = status_module.executeAction(request);
            REQUIRE(outcome.results.get<std::string>("stdout") == "***OUTPUT\n");
        }

        SECTION("it returns the action error string") {
            auto outcome = status_module.executeAction(request);
            REQUIRE(outcome.results.get<std::string>("stderr") == "***ERROR\n");
        }

        boost::filesystem::remove(symlink_path);
    }
}

}  // namespace CthunAgent
