cmake_minimum_required(VERSION 2.8.12)
project(cthun-agent)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Defaulting to a release build.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# define further options
option(EXTERNAL_CTHUN_CLIENT "ON - use an installed version of cthun-client. OFF - use the git submodule" ON)
option(TEST_VIRTUAL "ON - certain class member functions became virtual to enable mocking for unit tests" OFF)
option(LOG_COLOR "Enable colorization for logging" OFF)

# Project Output Paths

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(VENDOR_DIRECTORY ${PROJECT_SOURCE_DIR}/vendor)
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# set a version macro and expand related template
set(APPLICATION_VERSION_STRING "0.0.1")
configure_file(templates/version-inl.hpp ${CMAKE_BINARY_DIR}/generated/version-inl.hpp)

# set the root path macro and expand related template
set(ROOT_PATH ${PROJECT_SOURCE_DIR})
configure_file(templates/root_path.hpp ${CMAKE_BINARY_DIR}/generated/root_path.hpp)

# include cmake binaries for templates
include_directories(${CMAKE_BINARY_DIR}/generated)

# prefer openssl from ports
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include)
endif()

# Set the client library's logging prefix
set(CTHUN_CLIENT_LOGGING_PREFIX "puppetlabs.cthun_agent")

# Leatherman it up
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/vendor/leatherman/cmake")
include(options)

# Set compiler flags
include(cflags) # Leatherman
set(CTHUN_AGENT_FLAGS "")
set(CMAKE_CXX_FLAGS "${LEATHERMAN_CXX_FLAGS} ${CTHUN_AGENT_FLAGS}")

set(LEATHERMAN_USE_LOCALE TRUE)
set(LEATHERMAN_USE_NOWIDE TRUE)
set(LEATHERMAN_USE_CATCH TRUE)
set(LEATHERMAN_USE_RAPIDJSON TRUE)
set(LEATHERMAN_USE_JSON_CONTAINER TRUE)
set(LEATHERMAN_USE_LOGGING TRUE)
set(LEATHERMAN_USE_FILE_UTIL TRUE)
set(LEATHERMAN_USE_UTIL TRUE)
leatherman_logging_line_numbers()
add_subdirectory("vendor/leatherman")

if(LOG_COLOR)
    add_definitions(-DLOG_COLOR)
endif()

# Find libraries

find_package(Boost 1.54 REQUIRED
  COMPONENTS filesystem system date_time thread log regex random)

find_package(OpenSSL REQUIRED)

find_package(facter REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/boost-process.cmake)
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)

if(EXTERNAL_CTHUN_CLIENT)
    find_package(cthun-client REQUIRED)
else()
    include(${VENDOR_DIRECTORY}/cthun-client.cmake)
endif()

# Add the main binary

add_subdirectory(lib)
add_subdirectory(exe)

# Add the test suite

if(TEST_VIRTUAL)
    add_definitions(-DTEST_VIRTUAL)
endif()

enable_testing()

add_test(
    NAME "cthun-agent\\ library\\ tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/cthun-agent-unittests"
)

# Add cpplint target
FILE (GLOB_RECURSE ALL_SOURCES lib/*.cc lib/*.hpp exe/*.cc exe/*.hpp)
add_cpplint_files(${ALL_SOURCES})
enable_cpplint()
