#include <pxp-agent/modules/command.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <leatherman/file_util/file.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#ifndef _WIN32
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif
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
    #ifdef _WIN32
        // We use powershell for windows because this will match bolt's behavior:
        // bolt uses WinRM as a transport for windows, and WinRM uses powershell.exe
        // (and this behavior isn't believed to be configurable for WinRM)
        std::string shell_program = "powershell.exe";
        auto arguments = Util::splitArguments(raw_command);
        arguments.insert(arguments.begin(), { "-NoProfile", "-NonInteractive", "-NoLogo", "-ExecutionPolicy", "Bypass", "-Command" });
    #else
        // The data structures required here are all for getpwuid_r:
        // pwd <- The structure returned by getpwuid is stored here
        // buf <- The actual password entries that pwd points to are stored here
        // pw_entry_result <- This will either: 1. point to pwd if the call to getpwuid was successful,
        //                    or 2. point to NULL if the call wasn't successful.
        struct passwd pwd;
        struct passwd* pw_entry_result;
        auto buf = std::vector<char>(1024);
        // We fetch the shell interpreter using https://linux.die.net/man/3/getpwuid_r
        while (getpwuid_r(getuid(), &pwd, buf.data(), buf.size(), &pw_entry_result) == ERANGE) {
            buf.resize(buf.size() * 2);
        }
        if (!pw_entry_result) {
            // We should never get here, because the UID we are using is fetched from
            // the calling process which should always have an associated user. If we
            // do happen to get here, just fail hard.
            throw Module::ProcessingError(leatherman::locale::format("run_command thread failed to fetch user's passwd entry! cannot find a shell to run in, exiting..."));
        }
        std::string shell_program = pw_entry_result->pw_shell;
        std::vector<std::string> arguments;
        arguments.push_back("-c");
        arguments.push_back(raw_command);
    #endif
    const fs::path& results_dir { request.resultsDir() };

    Util::CommandObject cmd {
        // We move the shell_program var because technically it's
        // value points to a piece of memory that might get destroyed
        // after leaving the scope of the buildCommandObject function.
        // so we just move it's value in to the CommandObject to be
        // extra careful
        move(shell_program),    // Executable
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
