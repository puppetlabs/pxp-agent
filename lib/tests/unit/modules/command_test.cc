#include "../../common/content_format.hpp"

#include <pxp-agent/modules/command.hpp>
#include <leatherman/execution/execution.hpp>

#include <catch.hpp>

using namespace PXPAgent;

namespace lth_exe = leatherman::execution;
namespace lth_jc = leatherman::json_container;

namespace PXPAgent { namespace Modules {
    class MockedCommand : public Command {
    public:
        // Mock the execution result that run() will return
        leatherman::execution::result _mock_result;

        MockedCommand(bool success, std::string output, std::string error, int exit_code, size_t pid)
            : _mock_result(leatherman::execution::result { success, std::move(output), std::move(error), exit_code, pid }){
        }

        lth_exe::result run(const ActionRequest& request) override {
            return _mock_result;
        }
    };
}}

TEST_CASE("Modules::Command", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Command());
    }
}

TEST_CASE("Modules::Command::hasAction", "[modules]") {
    Modules::Command mod;

    SECTION("correctly reports false for a nonexistent action") {
        REQUIRE_FALSE(mod.hasAction("foo"));
    }

    SECTION("correctly reports true for a real action") {
        REQUIRE(mod.hasAction("run"));
    }
}

TEST_CASE("Modules::Command::callAction", "[modules]") {
    Modules::MockedCommand mod { true, "root", "", 0, 9999 };

    SECTION("executes a command") {
        auto command_txt = (DATA_FORMAT % "\"0987\""
                % "\"command\""
                % "\"run\""
                % "{\"command\": \"whoami\"}").str();
        PCPClient::ParsedChunks command_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(command_txt),
            {},
            0
        };

        ActionRequest request { RequestType::Blocking, command_content };
        auto response = mod.executeAction(request);
        auto output = response.action_metadata.toString();
        auto stdout = response.action_metadata.get<std::string>({"results", "stdout"});

        REQUIRE(stdout == "root");
    };
}
