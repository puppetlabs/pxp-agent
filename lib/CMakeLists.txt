# Set include directories

include_directories(
    inc
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

set(LIBRARY_COMMON_SOURCES
    src/action_request.cc
    src/agent.cc
    src/configuration.cc
    src/pxp_connector.cc
    src/external_module.cc
    src/module.cc
    src/modules/echo.cc
    src/modules/ping.cc
    src/modules/status.cc
    src/request_processor.cc
    src/pxp_schemas.cc
    src/thread_container.cc
)

if (UNIX)
    set(LIBRARY_STANDARD_SOURCES
        src/util/posix/daemonize.cc
        src/util/posix/pid_file.cc
        src/configuration/posix/configuration.cc
    )
endif()

if (WIN32)
    set(LIBRARY_STANDARD_SOURCES
        src/util/windows/daemonize.cc
        src/configuration/windows/configuration.cc
    )
endif()

set(LIBS
    ${cpp-pcp-client_LIBRARY}
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    ${LEATHERMAN_LIBRARIES}
)

if (WIN32)
    set(LIBPXP_AGENT_INSTALL_DESTINATION bin)
else()
    set(LIBPXP_AGENT_INSTALL_DESTINATION lib)
endif()

add_library(libpxp-agent SHARED ${LIBRARY_COMMON_SOURCES} ${LIBRARY_STANDARD_SOURCES})
target_link_libraries(libpxp-agent PRIVATE ${LIBS})
set_target_properties(libpxp-agent PROPERTIES PREFIX "" SUFFIX ".so" IMPORT_PREFIX "" IMPORT_SUFFIX ".so.a")

# Generate the export header
include(GenerateExportHeader)
generate_export_header(libpxp-agent EXPORT_FILE_NAME "${CMAKE_CURRENT_LIST_DIR}/inc/pxp-agent/export.h")

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    add_compiler_export_flags()
elseif (WIN32)
    add_compiler_export_flags()
    add_definitions("-Dlibpxp_agent_EXPORTS")
endif()

install(TARGETS libpxp-agent DESTINATION "${LIBPXP_AGENT_INSTALL_DESTINATION}")

add_subdirectory(tests)
