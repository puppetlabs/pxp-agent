cmake_minimum_required(VERSION 2.8.12)
project(cthun-agent)

# Project Output Paths

set(MAINFOLDER ${PROJECT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
set(LIBRARY_OUTPUT_PATH "${MAINFOLDER}/bin")
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
set(VENDOR_DIRECTORY "${PROJECT_SOURCE_DIR}/vendor")
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})

# set a version
set(APPLICATION_VERSION_STRING "0.0.1")

# expand a version template
configure_file(templates/version-inl.h ${CMAKE_BINARY_DIR}/generated/version-inl.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ )

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
leatherman_logging_line_numbers()
add_subdirectory("vendor/leatherman")

# Find libraries

find_package(Boost 1.54 REQUIRED
  COMPONENTS filesystem system date_time thread log regex random)

find_package(OpenSSL REQUIRED)

find_package(facter REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/boost-process.cmake)
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)
include(${VENDOR_DIRECTORY}/inih.cmake)
include(${VENDOR_DIRECTORY}/cthun-client.cmake)

# Add the main binary

add_subdirectory(lib)
add_subdirectory(exe)

# Add the test suite

enable_testing()

add_test(
    NAME "cthun-agent\\ library\\ tests"
    COMMAND "${MAINFOLDER}/bin/cthun-agent-unittests"
)

# Add cpplint target
FILE (GLOB_RECURSE ALL_SOURCES lib/*.cc lib/*.hpp exe/*.cc exe/*.hpp)
add_cpplint_files(${ALL_SOURCES})
enable_cpplint()
