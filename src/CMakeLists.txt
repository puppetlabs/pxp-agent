cmake_minimum_required(VERSION 2.8.12)

# allow include directives with absolute paths
set(BASEPATH "${CMAKE_CURRENT_LIST_DIR}/..")
include_directories("${BASEPATH}")

set(APPLICATION_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

set(LIBRARY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/data_container.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/configuration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/schemas.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/external_module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/inventory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/ping.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/status.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/file_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/string_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/thread_container.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/endpoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/connection_timings.cpp"
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
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    ${facter_LIBRARY}
)

add_library(cthun-agent ${LIBRARY_SOURCES})
target_link_libraries(cthun-agent ${LIBS})

add_executable(cthun-agent-bin ${APPLICATION_SOURCES})
set_target_properties(cthun-agent-bin
    PROPERTIES
    OUTPUT_NAME cthun-agent
    CLEAN_DIRECT_OUTPUT 1
)
target_link_libraries(cthun-agent-bin cthun-agent)

install(TARGETS cthun-agent-bin DESTINATION bin)
