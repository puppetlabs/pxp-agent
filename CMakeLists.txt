cmake_minimum_required(VERSION 3.2.2)
project(pxp-agent VERSION 1.15.19)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Defaulting to a release build.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# Define further options
option(DEV_LOG_COLOR "Enable colorization for logging (development setting)" OFF)

if(DEV_LOG_COLOR)
    add_definitions(-DDEV_LOG_COLOR)
endif()

# Project Output Paths
set(MODULES_INSTALL_PATH pxp-agent/modules CACHE STRING  "Location to install core modules. Can be an absolute path, or relative path from CMAKE_INSTALL_PREFIX.")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(VENDOR_DIRECTORY ${PROJECT_SOURCE_DIR}/vendor)
list(APPEND CMAKE_MODULE_PATH ${VENDOR_DIRECTORY})
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# expand template based on PROJECT_VERSION
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
set(LEATHERMAN_COMPONENTS locale nowide catch logging rapidjson json_container util file_util execution curl)

if (WIN32)
    list(APPEND LEATHERMAN_COMPONENTS windows)
endif()

find_package(Leatherman REQUIRED COMPONENTS ${LEATHERMAN_COMPONENTS})

# Include LEATHERMAN_USE_LOCALES
include(options)

if (LEATHERMAN_USE_LOCALES)
    # TODO(ale): enable i18n with add_definitions(-DLEATHERMAN_I18N) - PCP-257
    # add_definitions(-DLEATHERMAN_I18N)
    SET(BOOST_COMPONENTS locale)
endif()
LIST(APPEND BOOST_COMPONENTS filesystem chrono system date_time thread log regex random)

find_package(Boost 1.54 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
find_package(CPPHOCON REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpp-pcp-client REQUIRED)

# Specify the .cmake files for vendored libraries
include(${VENDOR_DIRECTORY}/horsewhisperer.cmake)

# Pull in helper macros for working with leatherman libraries
include(leatherman)

# Leatherman it up
include(cflags)
leatherman_logging_line_numbers()

# Set leatherman's flags
set(CMAKE_CXX_FLAGS "${LEATHERMAN_CXX_FLAGS}")
add_definitions(${LEATHERMAN_DEFINITIONS})

option(DYNAMICBASE "Add dynamicbase linker option" ON)
if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--nxcompat")
      if (DYNAMICBASE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--dynamicbase")
      endif()
endif()

#set CHECK_CXX_SOURCE_COMPILES environment to use leatherman
set(CMAKE_REQUIRED_FLAGS "-std=c++11")
set(CMAKE_REQUIRED_LIBRARIES ${LEATHERMAN_LIBRARIES})
set(CMAKE_REQUIRED_INCLUDES ${LEATHERMAN_INCLUDE_DIRS})

CHECK_CXX_SOURCE_COMPILES("
#define LEATHERMAN_LOGGING_NAMESPACE \"puppetlabs.pxp_agent.main\"
#include <leatherman/logging/logging.hpp>

int main() {
    leatherman::logging::setup_eventlog_logging(\"pxp-agent\");
    return 0;
}
" HAS_LTH_EVENTLOG)

if (HAS_LTH_EVENTLOG)
    add_definitions(-DHAS_LTH_EVENTLOG)
endif()

CHECK_CXX_SOURCE_COMPILES("
#define LEATHERMAN_LOGGING_NAMESPACE \"puppetlabs.pxp_agent.main\"
#include <leatherman/logging/logging.hpp>

using namespace leatherman::logging;

int main() {
    setup_syslog_logging(\"pxp-agent\", \"local5\");
    return 0;
}
" HAS_LTH_SYSLOG)

if (HAS_LTH_SYSLOG)
    add_definitions(-DHAS_LTH_SYSLOG)
endif()

# TODO(ale): enable translation with set(LEATHERMAN_LOCALES "...;...") and
# gettext_compile(${CMAKE_CURRENT_SOURCE_DIR}/locales share/locale)

# Configure i18n
file(GLOB_RECURSE PXP_AGENT_SOURCES lib/src/*.cc lib/inc/*.hpp exe/*.cc)
gettext_templates(${CMAKE_CURRENT_SOURCE_DIR}/locales ${PXP_AGENT_SOURCES})

# Display a summary of the features
include(FeatureSummary)
feature_summary(WHAT ALL)

# Add the test suite
enable_testing()

# Add the main binary
add_subdirectory(lib)
add_subdirectory(exe)

# Add the module install (the exe install is handled in the exe subdir)
install(FILES modules/pxp-module-puppet
        DESTINATION ${MODULES_INSTALL_PATH}
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if (WIN32)
# Add the module install batch file on windows platforms
install(FILES modules/pxp-module-puppet.bat
        DESTINATION ${MODULES_INSTALL_PATH}
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

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
