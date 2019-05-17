cmake_minimum_required(VERSION 3.2.2)

# Set include directories

include_directories(
    inc
    ${LEATHERMAN_INCLUDE_DIRS}
    ${CPPHOCON_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

set(LIBRARY_COMMON_SOURCES
    src/action_request.cc
    src/action_response.cc
    src/agent.cc
    src/configuration.cc
    src/external_module.cc
    src/module.cc
    src/pxp_connector_v1.cc
    src/pxp_connector_v2.cc
    src/pxp_schemas.cc
    src/request_processor.cc
    src/results_mutex.cc
    src/results_storage.cc
    src/thread_container.cc
    src/time.cc
    src/modules/echo.cc
    src/modules/ping.cc
    src/modules/task.cc
    src/util/utf8.cc
)

if (UNIX)
    set(LIBRARY_STANDARD_SOURCES
        src/util/posix/daemonize.cc
        src/util/posix/pid_file.cc
        src/util/posix/process.cc
        src/configuration/posix/configuration.cc
    )
endif()

if (WIN32)
    set(LIBRARY_STANDARD_SOURCES
        src/util/windows/daemonize.cc
        src/util/windows/process.cc
        src/configuration/windows/configuration.cc
    )
endif()

set (LIBS
    ${cpp-pcp-client_LIBRARY}
    ${CPPHOCON_LIBRARIES}
)

# Static libraries should come before shared libraries, or you can end up
# with some really annoying link errors. However, we can configure whether
# Boost and Leatherman are linked statically or dynamically.
if (LEATHERMAN_SHARED)
    # If Leatherman is shared, Boost should come first because
    # it's static, or the order doesn't matter.
    list(APPEND LIBS ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
else()
    # If Leatherman is static, it should come first as it depends
    # on Boost.
    list(APPEND LIBS ${LEATHERMAN_LIBRARIES} ${Boost_LIBRARIES})
endif()

list(APPEND LIBS ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})

if (WIN32)
    # Necessary when statically linking cpp-pcp-client on Windows.
    # Shouldn't hurt when cpp-pcp-client is a DLL.
    list(APPEND LIBS Ws2_32)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "SunOS" OR CMAKE_SYSTEM_NAME MATCHES "AIX")
    # On some platforms Boost.Thread has a dependency on clock_gettime. It also depends on pthread,
    # and FindBoost in CMake 3.2.3 doesn't include that dependency.
    find_package(Threads)
    list(APPEND LIBS rt ${CMAKE_THREAD_LIBS_INIT})
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND BOOST_STATIC AND LEATHERMAN_USE_LOCALES)
    # Needed with cpp-hocon using Boost.Locale
    list(APPEND LIBS iconv)
endif()

add_library(libpxp-agent STATIC ${LIBRARY_COMMON_SOURCES} ${LIBRARY_STANDARD_SOURCES})
add_dependencies(libpxp-agent horsewhisperer)
target_link_libraries(libpxp-agent ${LIBS})
set_target_properties(libpxp-agent PROPERTIES PREFIX "" IMPORT_PREFIX "")

install(TARGETS libpxp-agent
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

add_subdirectory(tests)
