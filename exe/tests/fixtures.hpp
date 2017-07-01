#include <boost/filesystem.hpp>
#include <vector>
#include <iostream>
#include <fstream>

using namespace std;
namespace fs = boost::filesystem;

// Provides a static temp directory for use by tests and the main implementation.
string test_prefix();

// A fixture that redirects stdin and stdout.
struct stream_fixture {
    stream_fixture(string input) {
        in.str(input);
        cinbuf = cin.rdbuf(in.rdbuf());
        coutbuf = cout.rdbuf(out.rdbuf());
    }

    ~stream_fixture() {
        cin.rdbuf(cinbuf);
        cout.rdbuf(coutbuf);
    }

    string output() {
        return out.str();
    }

private:
    stringstream in, out;
    streambuf *cinbuf, *coutbuf;
};

// Creates a unique temporary directory.
struct temp_directory {
    temp_directory() {
        dir = fs::absolute(fs::unique_path("task_fixture_%%%%-%%%%-%%%%-%%%%"));
        fs::create_directory(dir);
    }

    ~temp_directory() {
        fs::remove_all(dir);
    }

    string name() const {
        return dir.string();
    }

private:
    fs::path dir;
};

static string invalid_string()
{
    array<unsigned char, 3> n = {{0xed, 0xbf, 0xbf}};
    return string(n.begin(), n.end());
}

struct temp_task {
    temp_task(string mod) : module_path(test_prefix()+"/pxp-agent/tasks/"+mod+"/tasks") {
        fs::create_directories(module_path);
    }

    ~temp_task() {
        fs::remove_all(module_path);
    }

    void make_echo(string task = "init") {
        ofstream foo(module_path+"/"+task);
#ifdef _WIN32
        foo << "type -";
#else
        foo << "#!/bin/sh\ncat -";
#endif
    }

    void make_unparseable(string task = "init") {
        ofstream foo(module_path+"/"+task, ios_base::binary);
#ifndef _WIN32
        foo << "#!/bin/sh\n";
#endif
        foo << "echo " << invalid_string() << "\n";
    }

    void make_error(string task = "init") {
        ofstream foo(module_path+"/"+task, ios_base::binary);
#ifndef _WIN32
        foo << "#!/bin/sh\n";
#endif
        foo << "echo hello\n";
        foo << "echo goodbye 1>&2\n";
        foo << "exit 1\n";
    }

    void make_executable(string task = "init") {
#ifdef _WIN32
        fs::rename(module_path+"/"+task, module_path+"/"+task+".bat");
#else
        fs::permissions(module_path+"/"+task, fs::owner_read|fs::owner_write|fs::owner_exe);
#endif
    }

private:
    string module_path;
};

using lines = vector<string>;
static lines read(string filename)
{
    ifstream file(filename);
    lines output;
    string line;
    while (getline(file, line)) {
        output.push_back(line);
    }
    file.close();
    return output;
}

static string output_files(string const& dir)
{
    return "\"output_files\": {\"stdout\": \""+dir+"/out\", \"stderr\": \""+dir+"/err\", \"exitcode\": \""+dir+"/exit\"}";
}

static string basic_task(string task, string const& dir)
{
    return "{\"input\": {\"task\": \""+task+"\", \"input\": {}}, "+output_files(dir)+"}";
}

static bool validate_failure(string const& dir)
{
    auto err = read(dir+"/err");
    auto exi = read(dir+"/exit");
    return err.empty() && exi == lines{"255"};
}
