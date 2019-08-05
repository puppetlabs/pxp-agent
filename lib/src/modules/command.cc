#include <pxp-agent/modules/command.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <leatherman/file_util/file.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

namespace PXPAgent {
namespace Modules {

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_file = leatherman::file_util;

static const std::string COMMAND_RUN_ACTION { "run" };

Command::Command(const fs::path& exec_prefix, std::shared_ptr<ResultsStorage> storage) :
    BoltModule { exec_prefix, std::move(storage), nullptr }
{
    module_name = "command";
    actions.push_back(COMMAND_RUN_ACTION);

    // Command actions require a single "command" string parameter
    PCPClient::Schema input_schema { COMMAND_RUN_ACTION };
    input_schema.addConstraint("command", PCPClient::TypeConstraint::String, true);
    input_validator_.registerSchema(input_schema);

    PCPClient::Schema output_schema { COMMAND_RUN_ACTION };
    results_validator_.registerSchema(output_schema);
}

Util::CommandObject Command::buildCommandObject(const ActionRequest& request)
{
    const auto params = request.params();

    assert(params.includes("command") &&
           params.type("command") == lth_jc::DataType::String);

    auto raw_command = params.get<std::string>("command");
    auto chunks = Util::splitArguments(raw_command);
    auto command = *chunks.begin();
    std::vector<std::string> arguments { chunks.begin() + 1, chunks.end() };
    const fs::path& results_dir { request.resultsDir() };

    Util::CommandObject cmd {
        command,    // Executable
        arguments,  // Arguments
        {},         // Environment
        "",         // Input
        [results_dir](size_t pid) {
            auto pid_file = (results_dir / "pid").string();
            lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                           NIX_FILE_PERMS, std::ios::binary);
        }  // PID Callback
    };

    return cmd;
}

}  // namespace Modules
}  // namespace PXPAgent
