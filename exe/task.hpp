#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <functional>
#include <stdexcept>

struct task_error : std::runtime_error
{
    task_error(std::string t, std::string msg) :
        std::runtime_error(std::move(msg)), type(std::move(t)) {}
    std::string type;
};

struct task
{
    std::string name;
    std::vector<std::string> args;
};

struct task_runner
{
    task find_task(std::string taskname);

private:
    std::set<std::string> reserved_extensions_{".json", ".md"};
    // Hard-code extensions as executable on Windows. On non-Windows, we still rely on permissions and #!
    std::map<std::string, std::function<task(std::string)>> builtin_task_wrappers_{
#ifdef _WIN32
        {".rb",  [](std::string filename) { return task{"ruby", std::vector<std::string>{filename}}; }},
        {".pp",  [](std::string filename) { return task{"puppet", std::vector<std::string>{"apply", filename}}; }},
        {".ps1", [](std::string filename) { return task{"powershell",
                                                           std::vector<std::string>{
                                                               "-NoProfile",
                                                               "-NonInteractive",
                                                               "-NoLogo",
                                                               "-ExecutionPolicy",
                                                               "Bypass",
                                                               "-File",
                                                               filename
                                                           }}; }}
#endif
    };
};
