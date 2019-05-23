// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#include <leatherman/execution/execution.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <vector>
#include <fstream>

using namespace std;
namespace fs = boost::filesystem;
namespace lth_exec = leatherman::execution;
namespace lth_util = leatherman::util;

// Creates a unique temporary directory.
struct temp_directory {
    temp_directory() : dir{fs::absolute(fs::unique_path("task_fixture_%%%%-%%%%-%%%%-%%%%"))} {
        fs::create_directory(dir);
    }

    ~temp_directory() {
        fs::remove_all(dir);
    }

    string name() const {
        auto s = dir.string();
        boost::replace_all(s, "\\", "/");
        return s;
    }

private:
    fs::path dir;
};

static string read(string filename)
{
    ifstream file(filename);
    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

static fs::path exec_prefix;

static lth_exec::result execute(string const& input)
{
    return lth_exec::execute(
#ifdef _WIN32
        "cmd.exe",
        { "/c", (exec_prefix/"task_wrapper").string() },
#else
        (exec_prefix/"task_wrapper").string(),
        {},  // args
#endif
        input,
        0,   // timeout
        lth_util::option_set<lth_exec::execution_options> {
            lth_exec::execution_options::thread_safe,
            lth_exec::execution_options::merge_environment,
            lth_exec::execution_options::inherit_locale });
}

TEST_CASE("runs an executable") {
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    auto executable = dir+"/init.bat";
    auto input = "{\"executable\": \""+executable+"\", \"arguments\": [], "
        "\"input\": \"{\\\"a\\\": 1, \\\"b\\\": [2, 3], \\\"c\\\": {\\\"hello\\\": \\\"goodbye foo\\\"}}\", "
        "\"stdout\": \""+dir+"/out\", \"stderr\": \""+dir+"/err\", \"exitcode\": \""+dir+"/exit\"}";

    SECTION("passes input") {
        ofstream foo(executable);
#ifdef _WIN32
        foo << "@echo off" << endl;
        foo << "more" << endl;
#else
        foo << "#!/bin/sh" << endl;
        foo << "cat -" << endl;
#endif
        foo.close();

#ifndef _WIN32
        fs::permissions(executable, fs::owner_read|fs::owner_write|fs::owner_exe);
#endif

        auto exec = execute(input);
        REQUIRE(exec.output == "");
        REQUIRE(exec.error == "");
        REQUIRE(exec.exit_code == 0);

        auto output = read(dir+"/out");
        boost::trim(output);
        REQUIRE(output == "{\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}");

        REQUIRE(read(dir+"/err") == "");
        REQUIRE(read(dir+"/exit") == "0");
    }

    SECTION("errors if task not found") {
        auto exec = execute(input);
        REQUIRE(exec.output == "");
        REQUIRE(exec.error == "");
        REQUIRE(exec.exit_code == 127);

        REQUIRE(read(dir+"/out") == "");
        REQUIRE(read(dir+"/err") == "");
        REQUIRE(read(dir+"/exit") == "127");
    }

    SECTION("task not executable") {
        auto executable = dir+"/init";
        auto input = "{\"executable\": \""+executable+"\", \"arguments\": [], \"input\": \"\", "
            "\"stdout\": \""+dir+"/out\", \"stderr\": \""+dir+"/err\", \"exitcode\": \""+dir+"/exit\"}";
        ofstream foo(executable);
        foo << "";
        foo.close();

        auto exec = execute(input);
        REQUIRE(exec.output == "");
        REQUIRE(exec.error == "");
        REQUIRE(exec.exit_code == 127);

        REQUIRE(read(dir+"/out") == "");
        REQUIRE(read(dir+"/err") == "");
        REQUIRE(read(dir+"/exit") == "127");
    }
}

int main(int argc, char** argv) {
    exec_prefix = fs::absolute(fs::path(argv[0]).parent_path());

    // Create the Catch session, pass CL args, and start it
    Catch::Session test_session {};
    return test_session.run(argc, argv);
}

