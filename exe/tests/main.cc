// Refer to https://github.com/philsquared/Catch/blob/master/docs/own-main.md
// for providing our own main function to Catch
#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <array>
#include <boost/algorithm/string/erase.hpp>

#include "./fixtures.hpp"
#include "../task.hpp"

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
}

TEST_CASE("finds tasks") {
    task_runner runner;
    temp_directory tmpdir;
    auto dir = tmpdir.name();
    temp_task foo("foo");

    SECTION("simple name") {
        foo.make_echo();
        foo.make_executable("init", ".bat");

        auto t = runner.find_task("foo");
        boost::replace_all(t.name, "\\", "/");
        REQUIRE(t.name == foo.module_path+"/init.bat");
        REQUIRE(t.args == vector<string>{});
    }

    SECTION("compound task name") {
        foo.make_echo("bar");
        foo.make_executable("bar", ".bat");

        auto t = runner.find_task("foo::bar");
        boost::replace_all(t.name, "\\", "/");
        REQUIRE(t.name == foo.module_path+"/bar.bat");
        REQUIRE(t.args == vector<string>{});
    }

    SECTION("powershell task name") {
        foo.make_powershell({"a", "b", "c"});
        foo.make_executable("init", ".ps1");

        auto t = runner.find_task("foo");
#ifdef _WIN32
        REQUIRE(t.name == "powershell");
        auto args = vector<string>{"-NoProfile", "-NonInteractive", "-NoLogo", "-ExecutionPolicy", "Bypass", "-File", foo.module_path+"\\init.ps1"};
        REQUIRE(t.args == args);
#else
        REQUIRE(t.name == foo.module_path+"/init.ps1");
        REQUIRE(t.args == vector<string>{});
#endif
    }

    SECTION("ruby task name") {
        foo.make_ruby({"a", "b", "c"});
        foo.make_executable("init", ".rb");

        auto t = runner.find_task("foo");
#ifdef _WIN32
        REQUIRE(t.name == "ruby");
        auto args = vector<string>{foo.module_path+"\\init.rb"};
        REQUIRE(t.args == args);
#else
        REQUIRE(t.name == foo.module_path+"/init.rb");
        REQUIRE(t.args == vector<string>{});
#endif
    }

    SECTION("puppet task name") {
        foo.make_echo("bar");
        foo.make_executable("bar", ".pp");

        auto t = runner.find_task("foo::bar");
#ifdef _WIN32
        REQUIRE(t.name == "puppet");
        auto args = vector<string>{"apply", foo.module_path+"\\bar.pp"};
        REQUIRE(t.args == args);
#else
        REQUIRE(t.name == foo.module_path+"/bar.pp");
        REQUIRE(t.args == vector<string>{});
#endif
    }

    SECTION("ignores reserved files: json, markdown") {
        foo.make_echo();
        foo.make_executable("init", ".bat");
        foo.make_echo();
        foo.make_executable("init", ".json");
        foo.make_echo();
        foo.make_executable("init", ".md");

        auto t = runner.find_task("foo");
        boost::replace_all(t.name, "\\", "/");
        REQUIRE(t.name == foo.module_path+"/init.bat");
        REQUIRE(t.args == vector<string>{});
    }

    SECTION("from multiple tasks") {
        foo.make_echo("bar");
        foo.make_executable("bar", ".bat");

        foo.make_ruby({"a", "b", "c"});
        foo.make_executable("init", ".rb");

        auto t = runner.find_task("foo::bar");
        boost::replace_all(t.name, "\\", "/");
        REQUIRE(t.name == foo.module_path+"/bar.bat");
        REQUIRE(t.args == vector<string>{});
    }

    SECTION("errors when same task has multiple options") {
        foo.make_echo();
        foo.make_executable("init", ".bat");

        foo.make_ruby({"a", "b", "c"});
        foo.make_executable("init", ".rb");

        try {
            runner.find_task("foo");
            REQUIRE(false);
        } catch (task_error &e) {
            REQUIRE(e.type == "multiple-files");
            string msg = e.what();
            boost::replace_all(msg, "\\", "/");
            REQUIRE(msg == "Found multiple matching files for task 'foo': "
                    +foo.module_path+"/init.bat, "+foo.module_path+"/init.rb");
        }
    }

    SECTION("errors if task name is invalid") {
        SECTION("simple task name") {
            try {
                runner.find_task("foo!");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "invalid-task");
                REQUIRE(e.what() == string("Invalid task name 'foo!'"));
            }
        }

        SECTION("compound task name") {
            try {
                runner.find_task("fee:::fie");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "invalid-task");
                REQUIRE(e.what() == string("Invalid task name 'fee:::fie'"));
            }
        }
    }

    SECTION("errors if task not found") {
        SECTION("simple task name") {
            try {
                runner.find_task("foo");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "not-found");
                REQUIRE(e.what() == "Could not find task 'foo' at "+foo.module_path);
            }
        }

        SECTION("compound task name") {
            foo.make_echo();

            try {
                runner.find_task("foo::bar");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "not-found");
                REQUIRE(e.what() == "Could not find task 'foo::bar' at "+foo.module_path);
            }
        }

        SECTION("other task exists") {
            foo.make_echo("bar");

            try {
                runner.find_task("foo");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "not-found");
                REQUIRE(e.what() == "Could not find task 'foo' at "+foo.module_path);
            }
        }
    }

    SECTION("task not executable") {
        SECTION("simple task name") {
            foo.make_echo();

            try {
                runner.find_task("foo");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "not-executable");
                string msg = e.what();
                boost::replace_all(msg, "\\", "/");
                REQUIRE(msg == "Task file '"+foo.module_path+"/init' is not executable");
            }
        }

        SECTION("compound task name") {
            foo.make_echo("bar");

            try {
                runner.find_task("foo::bar");
                REQUIRE(false);
            } catch (task_error &e) {
                REQUIRE(e.type == "not-executable");
                string msg = e.what();
                boost::replace_all(msg, "\\", "/");
                REQUIRE(msg == "Task file '"+foo.module_path+"/bar' is not executable");
            }
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

