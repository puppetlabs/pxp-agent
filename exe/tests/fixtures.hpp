#include <boost/filesystem.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/algorithm/string/replace.hpp>
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
        cinbuf = boost::nowide::cin.rdbuf(in.rdbuf());
        coutbuf = boost::nowide::cout.rdbuf(out.rdbuf());
    }

    ~stream_fixture() {
        boost::nowide::cin.rdbuf(cinbuf);
        boost::nowide::cout.rdbuf(coutbuf);
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
        auto s = dir.string();
        boost::replace_all(s, "\\", "/");
        return s;
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
        foo << "@echo off" << endl;
        // More only prints a page, so this may fail with larger input.
        foo << "more" << endl;
#else
        foo << "#!/bin/sh" << endl;
        foo << "cat -" << endl;
#endif
    }

    void make_printer(vector<string> keys, string task = "init") {
        ofstream foo(module_path+"/"+task);
#ifndef _WIN32
        foo << "#!/bin/sh" << endl;
#endif
        for (auto& k : keys) {
#ifdef _WIN32
            foo << "@echo %PT_" << k << "%" << endl;
#else
            foo << "echo $PT_" << k << endl;
#endif
        }
    }

    void make_unparseable(string task = "init") {
        ofstream foo(module_path+"/"+task, ios_base::binary);
#ifdef _WIN32
        foo << "@echo off" << endl;
#else
        foo << "#!/bin/sh" << endl;
#endif
        foo << "echo " << invalid_string() << endl;
    }

    void make_error(string task = "init") {
        ofstream foo(module_path+"/"+task);
#ifdef _WIN32
        foo << "@echo off" << endl;
#else
        foo << "#!/bin/sh" << endl;
#endif
        foo << "echo hello" << endl;
        foo << "echo goodbye 1>&2" << endl;
        foo << "exit 1" << endl;
    }

    void make_powershell(vector<string> keys, string task = "init") {
        ofstream foo(module_path+"/"+task);
        foo << "#!/usr/bin/env powershell" << endl;
        foo << "foreach ($i in $input) { Write-Output $i }" << endl;
        for (auto& k : keys) {
            foo << "Write-Output $env:PT_" << k << endl;
        }
    }

    void make_ruby(vector<string> keys, string task = "init") {
        ofstream foo(module_path+"/"+task);
        foo << "#!/usr/bin/env ruby" << endl;
        foo << "puts STDIN.gets" << endl;
        for (auto& k : keys) {
            foo << "puts ENV['PT_" << k << "']" << endl;
        }
    }

    void make_executable(string task = "init", string ext = "") {
#ifdef _WIN32
        if (ext.empty()) {
            ext = ".bat";
        }
#else
        fs::permissions(module_path+"/"+task, fs::owner_read|fs::owner_write|fs::owner_exe);
#endif
        if (!ext.empty()) {
            fs::rename(module_path+"/"+task, module_path+"/"+task+ext);
        }
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
