// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <array>
#include <boost/algorithm/string/erase.hpp>

#include "./fixtures.hpp"

string test_prefix()
{
    static temp_directory temp_dir;
    return temp_dir.name();
}

int MAIN_IMPL(int argc, char** argv);

TEST_CASE("prints metadata") {
    stream_fixture fix("");
    auto args = array<char*, 2>{{const_cast<char*>("test_task"), const_cast<char*>("metadata")}};
    MAIN_IMPL(args.size(), args.data());
    auto metadata = R"(
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
    REQUIRE(fix.output() == metadata);
}

TEST_CASE("runs a simple task") {
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    auto args = array<char*, 1>{{const_cast<char*>("test_task")}};
    temp_task foo("foo");

    SECTION("simple task name") {
        stream_fixture fix("{\"input\": {\"task\": \"foo\", \"input\": {\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_echo();
        foo.make_executable();
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        // Strip newlines on Windows
        boost::erase_all(output[0], "\\r");
        boost::erase_all(output[0], "\\n");
        REQUIRE(output[0] == "{\"output\":\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":{\\\"hello\\\":\\\"goodbye foo\\\"}}\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }

    SECTION("compound task name") {
        stream_fixture fix("{\"input\": {\"task\": \"foo::bar\", \"input\": {\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_echo("bar");
        foo.make_executable("bar");
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        // Strip newlines on Windows
        boost::erase_all(output[0], "\\r");
        boost::erase_all(output[0], "\\n");
        REQUIRE(output[0] == "{\"output\":\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":{\\\"hello\\\":\\\"goodbye foo\\\"}}\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }

    SECTION("task with extension") {
        stream_fixture fix("{\"input\": {\"task\": \"foo\", \"input\": {\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_echo();
        foo.make_executable("init", ".bat");
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        // Strip newlines on Windows
        boost::erase_all(output[0], "\\r");
        boost::erase_all(output[0], "\\n");
        REQUIRE(output[0] == "{\"output\":\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":{\\\"hello\\\":\\\"goodbye foo\\\"}}\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }

#ifdef _WIN32
    SECTION("powershell task") {
        stream_fixture fix("{\"input\": {\"task\": \"foo\", \"input\": {\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_powershell({"a", "b", "c"});
        foo.make_executable("init", ".ps1");
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        // Strip newlines on Windows
        boost::erase_all(output[0], "\\r");
        REQUIRE(output[0] == "{\"output\":\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":{\\\"hello\\\":\\\"goodbye foo\\\"}}\\n1\\n[2,3]\\n{\\\"hello\\\":\\\"goodbye foo\\\"}\\n\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }
#endif

    SECTION("ruby task") {
        stream_fixture fix("{\"input\": {\"task\": \"foo\", \"input\": {\"a\": 1, \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_ruby({"a", "b", "c"});
        foo.make_executable("init", ".rb");
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        // Strip newlines on Windows
        boost::erase_all(output[0], "\\r");
        REQUIRE(output[0] == "{\"output\":\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":{\\\"hello\\\":\\\"goodbye foo\\\"}}\\n1\\n[2,3]\\n{\\\"hello\\\":\\\"goodbye foo\\\"}\\n\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }

    SECTION("sets input as environment variables in task") {
        stream_fixture fix("{\"input\": {\"task\": \"foo\", \"input\": {\"a\": \"1\", \"b\": [2, 3], \"c\": {\"hello\": \"goodbye foo\"}}}," + output_files(dir) + "}");

        foo.make_printer({"a", "b", "c"});
        foo.make_executable();
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output.size() == 1);
        boost::erase_all(output[0], "\\r");
        REQUIRE(output[0] == "{\"output\":\"1\\n[2,3]\\n{\\\"hello\\\":\\\"goodbye foo\\\"}\\n\"}");

        output = read(dir+"/err");
        REQUIRE(output.empty());

        output = read(dir+"/exit");
        REQUIRE(output == lines{"0"});
    }
}

TEST_CASE("returns an error if the task name is invalid") {
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    auto args = array<char*, 1>{{const_cast<char*>("test_task")}};

    SECTION("simple task name") {
        stream_fixture fix(basic_task("foo!", dir));
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/invalid-task","msg":"Invalid task name 'foo!'"}})"});
        REQUIRE(validate_failure(dir));
    }

    SECTION("compound task name") {
        stream_fixture fix(basic_task("fee:::fie", dir));
        MAIN_IMPL(args.size(), args.data());

        auto output = read(dir+"/out");
        REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/invalid-task","msg":"Invalid task name 'fee:::fie'"}})"});
        REQUIRE(validate_failure(dir));
    }
}

TEST_CASE("returns an error if the task can't be run") {
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    auto args = array<char*, 1>{{const_cast<char*>("test_task")}};

    SECTION("task not found") {
        SECTION("simple task name") {
            stream_fixture fix(basic_task("foo", dir));
            MAIN_IMPL(args.size(), args.data());

            auto output = read(dir+"/out");
            REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/not-found","msg":"Could not find task 'foo' at )"
                +test_prefix()+R"(/pxp-agent/tasks/foo/tasks"}})"});
            REQUIRE(validate_failure(dir));
        }

        SECTION("compound task name") {
            stream_fixture fix(basic_task("foo::bar", dir));
            MAIN_IMPL(args.size(), args.data());

            auto output = read(dir+"/out");
            REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/not-found","msg":"Could not find task 'foo::bar' at )"
                +test_prefix()+R"(/pxp-agent/tasks/foo/tasks"}})"});
            REQUIRE(validate_failure(dir));
        }

        SECTION("other task exists") {
            temp_task bar("foo");
            bar.make_echo("bar");

            stream_fixture fix(basic_task("foo", dir));
            MAIN_IMPL(args.size(), args.data());

            auto output = read(dir+"/out");
            REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/not-found","msg":"Could not find task 'foo' at )"
                +test_prefix()+R"(/pxp-agent/tasks/foo/tasks"}})"});
            REQUIRE(validate_failure(dir));
        }
    }

    SECTION("task not executable") {
        temp_task foo("foo");

        SECTION("simple task name") {
            foo.make_echo();
            stream_fixture fix(basic_task("foo", dir));
            MAIN_IMPL(args.size(), args.data());

            auto output = read(dir+"/out");
            REQUIRE(output.size() == 1);
            // Make pathing consistent.
            boost::replace_all(output[0], "\\\\", "/");
            REQUIRE(output[0] == R"({"_error":{"kind":"puppetlabs.tasks/not-executable","msg":"Task file ')"+test_prefix()+
                R"(/pxp-agent/tasks/foo/tasks/init' is not executable"}})");
            REQUIRE(validate_failure(dir));
        }

        SECTION("compound task name") {
            foo.make_echo("bar");
            stream_fixture fix(basic_task("foo::bar", dir));
            MAIN_IMPL(args.size(), args.data());

            auto output = read(dir+"/out");
            REQUIRE(output.size() == 1);
            // Make pathing consistent.
            boost::replace_all(output[0], "\\\\", "/");
            REQUIRE(output[0] == R"({"_error":{"kind":"puppetlabs.tasks/not-executable","msg":"Task file ')"+test_prefix()+
                R"(/pxp-agent/tasks/foo/tasks/bar' is not executable"}})");
            REQUIRE(validate_failure(dir));
        }
    }
}

TEST_CASE("errors when given an invalid json string") {
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    auto args = array<char*, 1>{{const_cast<char*>("test_task")}};
    temp_task foo("foo");
    stream_fixture fix(basic_task("foo", dir));

    foo.make_unparseable();
    foo.make_executable();
    MAIN_IMPL(args.size(), args.data());

    auto output = read(dir+"/out");
    REQUIRE(output == lines{R"({"_error":{"kind":"puppetlabs.tasks/output-encoding-error","msg":"Output is not valid UTF-8"}})"});

    output = read(dir+"/err");
    REQUIRE(output.empty());

    output = read(dir+"/exit");
    REQUIRE(output == lines{"0"});
}

