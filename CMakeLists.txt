cmake_minimum_required(VERSION 3.2.2)
project(pxp-agent)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Defaulting to a release build.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# Define further options
option(TEST_VIRTUAL "ON - certain class member functions became virtual to enable mocking for unit tests" OFF)
option(DEV_LOG_COLOR "Enable colorization for logging (development setting)" OFF)

# Project Output Paths
set(MODULES_INSTALL_PATH pxp-agent/modules CACHE STRING  "Location to install core modules. Can be an absolute path, or relative path from CMAKE_INSTALL_PREFIX.")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(VENDOR_DIRECTORY ${PROJECT_SOURCE_DIR}/vendor)
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Set a version macro and expand related template
set(APPLICATION_VERSION_STRING "1.0.2")
configure_file(templates/version-inl.hpp ${CMAKE_BINARY_DIR}/generated/version-inl.hpp)

# Set the root path macro and expand related template
set(ROOT_PATH ${PROJECT_SOURCE_DIR})
configure_file(templates/root_path.hpp ${CMAKE_BINARY_DIR}/generated/root_path.hpp)

# Include cmake binaries for templates
include_directories(${CMAKE_BINARY_DIR}/generated)

# Prefer openssl from ports
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /opt/local/include)
endif()

# Set RPATH if not installing to a system library directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" INSTALL_IS_SYSTEM_DIR)
if ("${INSTALL_IS_SYSTEM_DIR}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()

# Find libraries
set(LEATHERMAN_COMPONENTS locale nowide catch logging rapidjson json_container util file_util execution)

if (WIN32)
    list(APPEND LEATHERMAN_COMPONENTS windows)
endif()

find_package(Leatherman REQUIRED COMPONENTS ${LEATHERMAN_COMPONENTS})
find_package(Boost 1.54 REQUIRED
  COMPONENTS filesystem system date_time thread log regex random)
find_package(OpenSSL REQUIRED)
find_package(cpp-pcp-client REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)

# Pull in helper macros for working with leatherman libraries
include(leatherman)

# Leatherman it up
include(options)
include(cflags)
leatherman_logging_line_numbers()

# Set compiler flags
set(PXP_AGENT_FLAGS "")
set(CMAKE_CXX_FLAGS "${LEATHERMAN_CXX_FLAGS} ${PXP_AGENT_FLAGS}")
add_definitions(${LEATHERMAN_DEFINITIONS})

if(DEV_LOG_COLOR)
    add_definitions(-DDEV_LOG_COLOR)
endif()

# Display a summary of the features
include(FeatureSummary)
feature_summary(WHAT ALL)

# Add the main binary
add_subdirectory(lib)
add_subdirectory(exe)

# Add the module install (the exe install is handled in the exe subdir)
install(FILES modules/pxp-module-puppet
        DESTINATION ${MODULES_INSTALL_PATH}
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Add the test suite
if(TEST_VIRTUAL)
    add_definitions(-DTEST_VIRTUAL)
endif()

enable_testing()

add_test(
    NAME "pxp-agent\\ library\\ tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/pxp-agent-unittests"
)

# Add cpplint target
FILE (GLOB_RECURSE ALL_SOURCES lib/*.cc lib/*.hpp exe/*.cc exe/*.hpp)
add_cpplint_files(${ALL_SOURCES})
enable_cpplint()

add_cppcheck_dirs("${PROJECT_SOURCE_DIR}/lib" "${PROJECT_SOURCE_DIR}/exe")
enable_cppcheck()
