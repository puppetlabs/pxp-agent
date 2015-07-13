cmake_minimum_required(VERSION 2.8.12)

# Set include directories

include_directories(
    inc
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cthun-client_INCLUDE_DIR}
    ${facter_INCLUDE_DIR}
)

set(LIBRARY_SOURCES
    src/action_request.cc
    src/agent.cc
    src/configuration.cc
    src/cthun_connector.cc
    src/external_module.cc
    src/module.cc
    src/modules/echo.cc
    src/modules/inventory.cc
    src/modules/ping.cc
    src/modules/status.cc
    src/request_processor.cc
    src/rpc_schemas.cc
    src/thread_container.cc

    ${INIH_SOURCE_DIR}/ini.c
    ${INIH_SOURCE_DIR}/cpp/INIReader.cpp
)

add_custom_command(OUTPUT ${INIH_SOURCE_DIR}/ini.c
                   COMMAND ""
                   DEPENDS inih)
add_custom_command(OUTPUT ${INIH_SOURCE_DIR}/cpp/INIReader.cpp
                   COMMAND ""
                   DEPENDS inih)

SET(LIBS
    ${cthun-client_LIBRARY}
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    ${facter_LIBRARY}
    ${LEATHERMAN_LIBRARIES}
)

add_library(libcthun-agent ${LIBRARY_SOURCES})
target_link_libraries(libcthun-agent ${LIBS})

add_subdirectory(tests)
