#include <leatherman/json_container/json_container.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/locale/locale.hpp>
#include <leatherman/util/regex.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.task"
#include <leatherman/logging/logging.hpp>

#include <boost/nowide/args.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/filesystem.hpp>

#include <rapidjson/rapidjson.h>

#include <vector>
#include <string>

using namespace std;
namespace lth_jc = leatherman::json_container;
namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_log  = leatherman::logging;
namespace lth_util = leatherman::util;
namespace fs = boost::filesystem;

#ifndef MAIN_IMPL
#define MAIN_IMPL main
#define SYSTEM_PREFIX system_prefix

#ifdef _WIN32
#include <leatherman/windows/windows.hpp>
#include <leatherman/windows/system_error.hpp>
#undef ERROR
#include <Shlobj.h>
static string system_prefix()
{
    using leatherman::windows::system_error;
    wchar_t szPath[MAX_PATH+1];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath))) {
        throw runtime_error(_("failure getting Windows AppData directory: {1}", system_error()));
    }
    return (fs::path(szPath) / "PuppetLabs").string();
}
#else
static string system_prefix()
{
    return "/opt/puppetlabs";
}
#endif

#else
#define SYSTEM_PREFIX test_prefix
string test_prefix();
#endif

static void set_error(lth_jc::JsonContainer &result, string kind, string msg)
{
    lth_jc::JsonContainer err;
    err.set<string>("kind", "puppetlabs.tasks/"+kind);
    err.set<string>("msg", msg);
    result.set("_error", err);
}

static bool isValidUTF8(std::string &s)
{
    rapidjson::StringStream source(s.data());
    rapidjson::InsituStringStream target(&s[0]);
    target.PutBegin();
    while (source.Tell() < s.size()) {
        if (!rapidjson::UTF8<char>::Validate(source, target)) {
            return false;
        }
    }
    return true;
}

int MAIN_IMPL(int argc, char** argv)
{
    boost::nowide::args arg_utf8(argc, argv);

    if (argc > 1 && argv[1] == string("metadata")) {
        boost::nowide::cout << R"(
{
    "actions": [
        {
            "description": "Run a task",
            "input": {
                "properties": {
                    "task": {
                        "type": "string"
                    },
                    "input": {
                        "type": "object"
                    }
                },
                "required": [
                    "task", "input"
                ],
                "type": "object",
                "additionalProperties": true
            },
            "name": "run",
            "results": {
                "type": "object"
            }
        }
    ],
    "description": "Task runner module"
}
)";
        return 0;
    }

    lth_log::setup_logging(boost::nowide::cerr);
    lth_log::set_level(lth_log::log_level::error);

    boost::nowide::cin >> std::noskipws;
    istream_iterator<char> it(boost::nowide::cin), end;
    string STDIN(it, end);
    auto args = lth_jc::JsonContainer(STDIN);

    auto taskname = args.get<string>({"input", "task"});
    auto input = args.get<lth_jc::JsonContainer>({"input", "input"});

    auto stdout_file = args.get<string>({"output_files", "stdout"});
    auto stderr_file = args.get<string>({"output_files", "stderr"});
    auto exitcode_file = args.get<string>({"output_files", "exitcode"});

    auto exitcode = 255;
    lth_jc::JsonContainer stdout_result;
    string stderr_str;

    string module, task = "init";
    if (!lth_util::re_search(taskname, boost::regex("\\A(\\w+)\\z"), &module) &&
        !lth_util::re_search(taskname, boost::regex("\\A(\\w+)::(\\w+)\\z"), &module, &task)) {
        set_error(stdout_result, "invalid-task", _("Invalid task name '{1}'", taskname));
    } else {
        try {
            auto filename = SYSTEM_PREFIX()+"/pxp-agent/tasks/"+module+"/tasks/"+task;

            if (lth_exec::which(filename).empty()) {
                set_error(stdout_result, "not-found", _("Task file '{1}' is not present or not executable", filename));
            } else {
                auto exec = lth_exec::execute(
                    filename,
                    {},  // arguments
                    input.toString(),
                    0,  // timeout
                    { lth_exec::execution_options::thread_safe,
                      lth_exec::execution_options::merge_environment,
                      lth_exec::execution_options::inherit_locale });

                if (!isValidUTF8(exec.output)) {
                    set_error(stdout_result, "output-encoding-error", _("Output cannot be represented as a JSON string"));
                } else {
                    stdout_result.set("output", exec.output);
                }

                stderr_str = exec.error;
                exitcode = exec.exit_code;
            }
        } catch (runtime_error &e) {
            set_error(stdout_result, "exec-failed", _("Task '{1}' failed to run: {2}", taskname, e.what()));
        }
    }

    lth_file::atomic_write_to_file(stdout_result.toString(), stdout_file);
    lth_file::atomic_write_to_file(stderr_str, stderr_file);
    lth_file::atomic_write_to_file(to_string(exitcode), exitcode_file);

    return 0;
}
