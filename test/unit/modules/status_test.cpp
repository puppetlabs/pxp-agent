#include <cstdio>

#include "test/test.h"

#include "src/errors.h"
#include "src/uuid.h"
#include "src/file_utils.h"
#include "src/modules/status.h"
#include "src/configuration.h"

#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

extern std::string ROOT_PATH;

// TODO(ale): update this to cthun-client

namespace CthunAgent {

static const std::string query_action { "query" };

boost::format status_format {
    "{   \"module\" : \"status\","
    "    \"action\" : \"query\","
    "    \"params\" : {\"job_id\" : \"%1%\"}"
    "}"
};

static const std::string status_txt { (status_format % "the-uuid-string").str() };

static const std::vector<DataContainer> no_debug {};

static const ParsedContent content { DataContainer(),
                                     DataContainer(status_txt),
                                     no_debug };

TEST_CASE("Modules::Status::callAction", "[modules]") {
    Modules::Status status_module {};

    SECTION("the status module is correctly named") {
        REQUIRE(status_module.module_name == "status");
    }

    SECTION("the inventory module has the 'query' action") {
        REQUIRE(status_module.actions.find(query_action)
                != status_module.actions.end());
    }

    SECTION("it can call the 'query' action") {
        REQUIRE_NOTHROW(status_module.callAction(query_action, content));
    }

    SECTION("it works properly when an unknown job id is provided") {
        auto job_id = UUID::getUUID();
        std::string other_status_txt { (status_format % job_id).str() };
        ParsedContent other_content { DataContainer(),
                                     DataContainer(other_status_txt),
                                     no_debug };

        SECTION("it doesn't throw") {
            REQUIRE_NOTHROW(status_module.callAction(query_action, other_content));
        }

        SECTION("it returns an error") {
            auto result = status_module.callAction(query_action, other_content);
            REQUIRE(result.includes("error"));
        }
    }

    SECTION("it correctly retrieves the file content of a known job") {
        std::string result_path { ROOT_PATH + "/test/resources/delayed_result" };
        boost::filesystem::path to { result_path };

        auto symlink_name = UUID::getUUID();
        std::string symlink_path { DEFAULT_ACTION_RESULTS_DIR + symlink_name };
        boost::filesystem::path symlink { symlink_path };

        std::string other_status_txt { (status_format % symlink_name).str() };
        ParsedContent other_content { DataContainer(),
                                     DataContainer(other_status_txt),
                                     no_debug };

        try {
            boost::filesystem::create_symlink(to, symlink);
        } catch (boost::filesystem::filesystem_error) {
            FAIL("Failed to create the symlink");
        }

        SECTION("it doesn't throw") {
            REQUIRE_NOTHROW(status_module.callAction(query_action, other_content));
        }

        SECTION("it returns the action status") {
            auto result = status_module.callAction(query_action, other_content);
            REQUIRE(result.get<std::string>("status") == "Completed");
        }

        SECTION("it returns the action output") {
            auto result = status_module.callAction(query_action, other_content);
            REQUIRE(result.get<std::string>("stdout") == "***OUTPUT\n");
        }

        SECTION("it returns the action error string") {
            auto result = status_module.callAction(query_action, other_content);
            REQUIRE(result.get<std::string>("stderr") == "***ERROR\n");
        }

        FileUtils::removeFile(symlink_path);
    }
}

}  // namespace CthunAgent
