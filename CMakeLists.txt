cmake_minimum_required(VERSION 2.8.12)
project(MCOLLECTIVE-ZEROMQ-BROKER)

# Project Output Paths

SET (MAINFOLDER ${PROJECT_SOURCE_DIR})
SET (EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
SET (LIBRARY_OUTPUT_PATH "${MAINFOLDER}/lib")

# Set compiler-specific flags

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-reorder -Wno-unused-parameter -Wno-deprecated-register")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wno-unused-parameter -Wno-unused-local-typedefs -Wno-ignored-qualifiers -Wno-reorder")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

set(MAINFOLDER ${PROJECT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")

set(VENDOR_DIRECTORY "${PROJECT_SOURCE_DIR}/vendor")
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})

# prefer openssl from ports
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include)
endif()

# Find libraries

find_package(Boost 1.54 REQUIRED
  COMPONENTS program_options filesystem system date_time thread log regex random)

find_package(OpenSSL REQUIRED)

# Specify the .cmake files for vendored libraries

# TODO(ale): cthun-agent won't compile with boost 1.56 due to valijson;
# must investigate valijson compatibility. Error:
# valijson/schema.hpp:177:16: error:
#   no viable conversion from 'const boost::optional<std::string>' to 'bool'
include(${VENDOR_DIRECTORY}/valijson.cmake)
include(${VENDOR_DIRECTORY}/gmock.cmake)
include(${VENDOR_DIRECTORY}/boost-process.cmake)
include(${VENDOR_DIRECTORY}/websocketpp.cmake)

# Set include directories

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${Boost_Process_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIRS}
    ${VALIJSON_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
    "${VENDOR_DIRECTORY}/jsoncpp"
)

# Add the main binary

add_subdirectory(src)

# Add the test suite

enable_testing()
add_subdirectory(test)

# Add cpplint target

include(FindPythonInterp)
if (NOT PYTHONINTERP_FOUND)
    message(STATUS "Python not found; 'cpplint' target will not be available")
else()
    set(CPPLINT_FILTER
        "-build/include"
        "-legal/copyright"
        "-runtime/references"
        "-build/header_guard"
        "-readability/streams"
        "-whitespace/line_length"
        "-build/c++11"
        "-whitespace/indent"
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
