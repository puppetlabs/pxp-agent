#include <leatherman/json_container/json_container.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/locale/locale.hpp>

#include <boost/nowide/iostream.hpp>
#include <boost/nowide/fstream.hpp>

namespace lth_loc = leatherman::locale;
namespace lth_jc = leatherman::json_container;
namespace lth_exec = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_util = leatherman::util;
namespace fs = boost::filesystem;

#ifndef _WIN32
static const fs::perms FILE_PERMS { fs::owner_read | fs::owner_write | fs::group_read };
#endif

int main(int argc, char *argv[])
{
    // Read JSON input from stdin. Input should take the following format:
    // {
    //    "executable": "(path to the executable to run)",
    //    "arguments": [...],
    //    "input": "(string to pass to the executable on stdin)",
    //    "stdout": "(filepath to write stdout to)",
    //    "stderr": "(filepath to write stderr to)",
    //    "exitcode": "(filepath to write exitcode to)"
    // }
    boost::nowide::cin >> std::noskipws;
    std::istream_iterator<char> i_s_i(boost::nowide::cin), end;
    auto params = lth_jc::JsonContainer(std::string { i_s_i, end });
    auto executable = params.get<std::string>("executable");
    int exitcode;

    try {
        auto exec = lth_exec::execute(
            executable,
            params.get<std::vector<std::string>>("arguments"),
            params.get<std::string>("input"),
            params.get<std::string>("stdout"),
            params.get<std::string>("stderr"),
            {},       // environment
            nullptr,  // PID callback
            0,        // timeout
#ifndef _WIN32
            // Not used on Windows. We instead rely on inherited directory ACLs.
            FILE_PERMS,
#endif
            lth_util::option_set<lth_exec::execution_options> {
                lth_exec::execution_options::thread_safe,
                lth_exec::execution_options::merge_environment,
                lth_exec::execution_options::allow_stdin_unread,
                lth_exec::execution_options::inherit_locale });
        exitcode = exec.exit_code;
    } catch (lth_exec::execution_exception &e) {
        // Avoid atomic update to allow testing against /dev/stderr. There should never be
        // multiple processes trying to write this output.
        boost::nowide::ofstream ofs { params.get<std::string>("stderr"), std::ios::binary };
#ifndef _WIN32
        fs::permissions(params.get<std::string>("stderr"), FILE_PERMS);
#endif
        ofs << lth_loc::format("Executable '{1}' failed to run: {2}", executable, e.what());
        exitcode = 127;
    }

    // Write the exit code; at this point, stdout and stderr are already written
#ifdef _WIN32
    lth_file::atomic_write_to_file(std::to_string(exitcode), params.get<std::string>("exitcode"));
#else
    lth_file::atomic_write_to_file(std::to_string(exitcode), params.get<std::string>("exitcode"),
                                   FILE_PERMS, std::ios::binary);
#endif

    return exitcode;
}
