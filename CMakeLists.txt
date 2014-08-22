cmake_minimum_required(VERSION 2.8.12)
project(MCOLLECTIVE-ZEROMQ-BROKER)


# Set compiler-specific flags
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-reorder -Wno-unused-parameter -Wno-deprecated-register")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-local-typedefs")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

set(MAINFOLDER ${PROJECT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")

set(VENDOR_DIRECTORY "${PROJECT_SOURCE_DIR}/vendor")
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})

find_package(Boost 1.54 REQUIRED
  COMPONENTS program_options filesystem system date_time thread log log_setup regex)

# Include vendored libraries
include(${VENDOR_DIRECTORY}/gmock.cmake)
include(${VENDOR_DIRECTORY}/valijson.cmake)
include(${VENDOR_DIRECTORY}/boost-process.cmake)

# Set include directories
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${Boost_Process_INCLUDE_DIRS}
    "${VENDOR_DIRECTORY}/jsoncpp"
    ${VALIJSON_INCLUDE_DIRS}
)

# Add the main binary
add_subdirectory(src)

# Add the test suite
add_subdirectory(test)
