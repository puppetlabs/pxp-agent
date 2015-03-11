cmake_minimum_required(VERSION 2.8.12)
project(cthun-agent C CXX)

# Project Output Paths

set(MAINFOLDER ${PROJECT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
set(LIBRARY_OUTPUT_PATH "${MAINFOLDER}/lib")
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
set(VENDOR_DIRECTORY "${PROJECT_SOURCE_DIR}/vendor")
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})

# Set compiler-specific flags

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-reorder -Wno-unused-parameter -Wno-deprecated-register")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # NB: -pthread is needed for fedora20 (cmake does not add the library, even
    # afer finding it)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-parameter -pthread")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

# prefer openssl from ports
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include)
endif()

# Find libraries

find_package(Boost 1.54 REQUIRED
  COMPONENTS filesystem system date_time thread log regex random)

find_package(OpenSSL REQUIRED)

find_package(facter REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/boost-process.cmake)
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)
include(${VENDOR_DIRECTORY}/inih.cmake)

# TODO(ale): integrate CthunClient in a better way; remove local libraries

# CthunClient macros
set (CTHUN_CLIENT_INCLUDE_DIRS "${VENDOR_DIRECTORY}/cthun-client")
set (CTHUN_CLIENT_SOURCE_DIR "${CTHUN_CLIENT_INCLUDE_DIRS}/cthun-client/src")
set (CTHUN_CLIENT_VENDOR_DIR "${CTHUN_CLIENT_INCLUDE_DIRS}/cthun-client/vendor")

include(${CTHUN_CLIENT_VENDOR_DIR}/rapidjson.cmake)
include(${CTHUN_CLIENT_VENDOR_DIR}/valijson.cmake)
include(${CTHUN_CLIENT_VENDOR_DIR}/websocketpp.cmake)

# Set include directories

include_directories(
    SYSTEM ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    SYSTEM ${HORSEWHISPERER_INCLUDE_DIRS}
    SYSTEM ${INIH_INCLUDE_DIRS}
    SYSTEM ${CTHUN_CLIENT_INCLUDE_DIRS}
    SYSTEM ${RAPIDJSON_INCLUDE_DIRS}
    SYSTEM ${VALIJSON_INCLUDE_DIRS}
    SYSTEM ${WEBSOCKETPP_INCLUDE_DIRS}
)

# Add the main binary

add_subdirectory(src)

# Add the test suite

enable_testing()
add_subdirectory(test)

add_test(
    NAME "cthun-agent\\ library\\ tests"
    COMMAND "${MAINFOLDER}/bin/cthun-agent-unittests"
)

# Add cpplint target

include(FindPythonInterp)
if (NOT PYTHONINTERP_FOUND)
    message(STATUS "Python not found; 'cpplint' target will not be available")
else()
    set(CPPLINT_FILTER
        "-build/c++11"            # <thread>, <condvar>, etc...
        "-whitespace/indent"      # We use 4 space indentation
        "-build/include"          # Why?
        "-build/namespaces"       # What's a namespace to do
        "-legal/copyright"        # Not yet
        "-runtime/references"     # Not sure about this religion
        "-readability/streams"    # What?
        "-readability/namespace"  # Ignore nested namespace comment formatting
        "-whitespace/braces"      # Is there a k&r setting?
        "-whitespace/line_length" # Well yeah, but ... not just now
        "-runtime/arrays"         # Sizing an array with a 'const int' doesn't make it variable sized
        "-readability/todo"       # Seriously? todo comments need to identify an owner? pffft
        "-whitespace/empty_loop_body" # Can't handle do { ... } while(expr);
        "-runtime/int"            # Some C types are needed for library interop
        "-runtime/explicit"       # Using implicit conversion from string to regex for regex calls.
        "-build/header_guard"     # Disable header guards (cpplint doesn't yet support enforcing #pragma once)
    )

    FILE (GLOB_RECURSE ALL_SOURCES src/*.cpp src/*.h test/*.cpp test/*.h)

    set(CPPLINT_PATH "${MAINFOLDER}/ext/cpplint.py")

    set(CPPLINT_ARGS "")
    if (CPPLINT_FILTER)
        string(REPLACE ";" "," CPPLINT_FILTER "${CPPLINT_FILTER}")
        set(CPPLINT_ARGS "${CPPLINT_ARGS}--filter=${CPPLINT_FILTER}")
    endif()
    if (MSVC)
        set(CPPLINT_ARGS "${CPPLINT_ARGS} --output=vs7")
    endif()

    ADD_CUSTOM_TARGET(cpplint
        COMMAND ${PYTHON_EXECUTABLE} ${CPPLINT_PATH} ${CPPLINT_ARGS} ${ALL_SOURCES}
        VERBATIM
    )
endif()
