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

int main(int argc, char *argv[])
{
    boost::nowide::cin >> std::noskipws;
    std::istream_iterator<char> i_s_i(boost::nowide::cin), end;
    auto params = lth_jc::JsonContainer(std::string { i_s_i, end });
    auto task_executable = params.get<std::string>("executable");
    int exitcode;

    try {
        auto exec = lth_exec::execute(
            task_executable,
            params.get<std::vector<std::string>>("arguments"),
            params.get<std::string>("input"),
            params.get<std::string>("stdout"),
            params.get<std::string>("stderr"),
            {},       // environment
            nullptr,  // PID callback
            0,        // timeout
            lth_util::option_set<lth_exec::execution_options> {
                lth_exec::execution_options::thread_safe,
                lth_exec::execution_options::merge_environment,
                lth_exec::execution_options::inherit_locale });
        exitcode = exec.exit_code;
    } catch (lth_exec::execution_exception &e) {
        lth_file::atomic_write_to_file(lth_loc::format("Task '{1}' failed to run: {2}", task_executable, e.what()),
                                       params.get<std::string>("stderr"));
        exitcode = 127;
    }

    lth_file::atomic_write_to_file(std::to_string(exitcode), params.get<std::string>("exitcode"));

    return exitcode;
}
