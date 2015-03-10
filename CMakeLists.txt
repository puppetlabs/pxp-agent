cmake_minimum_required(VERSION 2.8.12)
project(cthun-agent C CXX)

# Project Output Paths

SET (MAINFOLDER ${PROJECT_SOURCE_DIR})
SET (EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
SET (LIBRARY_OUTPUT_PATH "${MAINFOLDER}/lib")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/vendor/leatherman/cmake")

# Set compiler-specific flags
include(cflags)
set(CMAKE_CXX_FLAGS ${LEATHERMAN_CXX_FLAGS})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable -Wno-reorder")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable -Wno-reorder -pthread")
endif()

set(VENDOR_DIRECTORY "${PROJECT_SOURCE_DIR}/vendor")
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})

# prefer openssl from ports
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include)
endif()

# Load up common leatherman configuration, before we load any files
include(options)

# Find libraries

find_package(Boost 1.54 REQUIRED
  COMPONENTS filesystem system date_time thread log regex random)

find_package(OpenSSL REQUIRED)

find_package(facter REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/rapidjson.cmake)
include(${VENDOR_DIRECTORY}/boost-process.cmake)
include(${VENDOR_DIRECTORY}/websocketpp.cmake)
include(${VENDOR_DIRECTORY}/valijson.cmake)
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)
include(${VENDOR_DIRECTORY}/inih.cmake)

# Pull in leatherman libraries
set(LEATHERMAN_USE_LOCALE TRUE)
set(LEATHERMAN_USE_CATCH TRUE)
set(LEATHERMAN_USE_NOWIDE TRUE)
set(LEATHERMAN_USE_LOGGING TRUE)
add_subdirectory("vendor/leatherman")


# Set include directories

include_directories(
    SYSTEM ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    SYSTEM ${WEBSOCKETPP_INCLUDE_DIRS}
    SYSTEM ${VALIJSON_INCLUDE_DIRS}
    SYSTEM ${RAPIDJSON_INCLUDE_DIRS}
    SYSTEM ${HORSEWHISPERER_INCLUDE_DIRS}
    SYSTEM ${INIH_INCLUDE_DIRS}
    SYSTEM /home/branan/prefix/include
    ${LEATHERMAN_INCLUDE_DIRS}
)

# Add the main binary

add_subdirectory(src)

# Add the test suite

enable_testing()
add_subdirectory(test)

# Add cpplint target
FILE (GLOB_RECURSE ALL_SOURCES src/*.cpp src/*.h test/*.cpp test/*.h)
add_cpplint_files(${ALL_SOURCES})
enable_cpplint()