cmake_minimum_required(VERSION 3.2.2)

# Set include directories

include_directories(
    inc
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

set(LIBRARY_COMMON_SOURCES
    src/action_request.cc
    src/action_response.cc
    src/agent.cc
    src/configuration.cc
    src/external_module.cc
    src/module.cc
    src/pxp_connector.cc
    src/pxp_schemas.cc
    src/request_processor.cc
    src/results_mutex.cc
    src/results_storage.cc
    src/thread_container.cc
    src/time.cc
    src/modules/echo.cc
    src/modules/ping.cc
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
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${LEATHERMAN_LIBRARIES}
)

if (WIN32)
    # Necessary when statically linking cpp-pcp-client on Windows.
    # Shouldn't hurt when cpp-pcp-client is a DLL.
    list(APPEND LIBS Ws2_32)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "SunOS" OR CMAKE_SYSTEM_NAME MATCHES "AIX")
    # On some platforms Boost.Thread has a dependency on clock_gettime.
    list(APPEND LIBS rt)
endif()

add_library(libpxp-agent STATIC ${LIBRARY_COMMON_SOURCES} ${LIBRARY_STANDARD_SOURCES})
target_link_libraries(libpxp-agent ${LIBS})
set_target_properties(libpxp-agent PROPERTIES PREFIX "" IMPORT_PREFIX "")

install(TARGETS libpxp-agent
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

add_subdirectory(tests)
