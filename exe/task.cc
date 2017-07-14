#include "task.hpp"

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/locale/locale.hpp>
#include <leatherman/util/regex.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.task"
#include <leatherman/logging/logging.hpp>

#include <boost/nowide/iostream.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>

#include <rapidjson/rapidjson.h>

using namespace std;
namespace lth_jc = leatherman::json_container;
namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_log  = leatherman::logging;
namespace lth_util = leatherman::util;
namespace fs = boost::filesystem;

// Defines to allow testing the main function with an arbitrary task location.
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

task task_runner::find_task(string taskname) {
    string module, task = "init";
    if (!lth_util::re_search(taskname, boost::regex("\\A(\\w+)\\z"), &module) &&
        !lth_util::re_search(taskname, boost::regex("\\A(\\w+)::(\\w+)\\z"), &module, &task)) {
        throw task_error("invalid-task", _("Invalid task name '{1}'", taskname));
    }

    auto filepath = SYSTEM_PREFIX()+"/pxp-agent/tasks/"+module+"/tasks";

    // Search for any executable file starting with the task name and not matching a set of reserved extensions
    vector<string> filenames;
    string extension;

    try {
        fs::directory_iterator end;
        for (auto f = fs::directory_iterator(filepath); f != end; ++f) {
            if (!fs::is_directory(f->status()) && f->path().stem() == task) {
                auto ext = f->path().extension().string();
                if (reserved_extensions_.find(ext) != reserved_extensions_.end()) {
                    continue;
                }

                // Save for later. The last one is fine, since we will error if there's not exactly one file.
                extension = move(ext);
                filenames.emplace_back(f->path().string());
            }
        }
    } catch (fs::filesystem_error &e) {
        // Fall through to filenames.empty
    }

    if (filenames.empty()) {
        throw task_error("not-found", _("Could not find task '{1}' at {2}", taskname, filepath));
    }
    if (filenames.size() > 1) {
        throw task_error("multiple-files", _("Found multiple matching files for task '{1}': {2}", taskname,
                                             boost::algorithm::join(filenames, ", ")));
    }

    auto filename = filenames.front();

    auto builtin = builtin_task_wrappers_.find(extension);
    if (builtin != builtin_task_wrappers_.end()) {
        return builtin->second(filename);
    }

    if (lth_exec::which(filename).empty()) {
        throw task_error("not-executable", _("Task file '{1}' is not executable", filename));
    }
    return {filename, vector<string>{}};
}

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

static map<string, string> generate_environment_from(lth_jc::JsonContainer &input)
{
    map<string, string> env_vars;
    for (auto& k : input.keys()) {
        auto val = input.type(k) == lth_jc::String ? input.get<string>(k) : input.toString(k);
        env_vars.emplace("PT_"+k, move(val));
    }
    return env_vars;
}

int MAIN_IMPL(int argc, char** argv)
{
    // Fix args on Windows to be UTF-8. Disabled because it prevents testing
    // and we expect "metadata" as the only argument.
    // boost::nowide::args arg_utf8(argc, argv);

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
)" << std::flush;
        return 0;
    }

    lth_log::setup_logging(boost::nowide::cerr);
    if (argc > 1 && argv[1] == string("trace")) {
        lth_log::set_level(lth_log::log_level::trace);
    } else {
        lth_log::set_level(lth_log::log_level::error);
    }

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

    try {
        task_runner runner;
        auto task = runner.find_task(taskname);
        auto environment = generate_environment_from(input);

        auto exec = lth_exec::execute(
            task.name,
            task.args,
            input.toString(),
            environment,
            0,  // timeout
            { lth_exec::execution_options::thread_safe,
              lth_exec::execution_options::merge_environment,
              lth_exec::execution_options::inherit_locale });

        if (!isValidUTF8(exec.output)) {
            set_error(stdout_result, "output-encoding-error", _("Output is not valid UTF-8"));
        } else {
            stdout_result.set("output", exec.output);
        }

        stderr_str = exec.error;
        exitcode = exec.exit_code;
    } catch (task_error &e) {
        set_error(stdout_result, e.type, e.what());
    } catch (runtime_error &e) {
        set_error(stdout_result, "exec-failed", _("Task '{1}' failed to run: {2}", taskname, e.what()));
    }

    lth_file::atomic_write_to_file(stdout_result.toString(), stdout_file);
    lth_file::atomic_write_to_file(stderr_str, stderr_file);
    lth_file::atomic_write_to_file(to_string(exitcode), exitcode_file);

    return 0;
}
