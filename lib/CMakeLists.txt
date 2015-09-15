cmake_minimum_required(VERSION 2.8.12)

# Set include directories

include_directories(
    inc
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
    ${facter_INCLUDE_DIR}
)

set(LIBRARY_SOURCES
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

SET(LIBS
    ${cpp-pcp-client_LIBRARY}
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    ${facter_LIBRARY}
    ${LEATHERMAN_LIBRARIES}
)

if (WIN32)
    set(LIBPXP_AGENT_INSTALL_DESTINATION bin)
else()
    set(LIBPXP_AGENT_INSTALL_DESTINATION lib)
endif()

add_library(libpxp-agent ${LIBRARY_SOURCES})
target_link_libraries(libpxp-agent ${LIBS})
set_target_properties(libpxp-agent PROPERTIES PREFIX "" SUFFIX ".so" IMPORT_PREFIX "" IMPORT_SUFFIX ".so.a")

install(TARGETS libpxp-agent DESTINATION "${LIBPXP_AGENT_INSTALL_DESTINATION}")

add_subdirectory(tests)
