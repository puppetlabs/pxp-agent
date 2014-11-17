cmake_minimum_required(VERSION 2.8.12)

# allow include directives with absolute paths
set(BASEPATH "${CMAKE_CURRENT_LIST_DIR}/..")
include_directories("${BASEPATH}")

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/agent_endpoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/schemas.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/external_module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/inventory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/ping.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/status.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/file_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/string_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/endpoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/connection_timings.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
)

SET(LIBS
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    facter
)

add_library(cthun-agent ${SOURCES})
target_link_libraries(cthun-agent ${LIBS})

add_executable(cthun-agent-bin ${SOURCES})
set_target_properties(cthun-agent-bin
    PROPERTIES
    OUTPUT_NAME cthun-agent
    CLEAN_DIRECT_OUTPUT 1
)
target_link_libraries(cthun-agent-bin cthun-agent)

install(TARGETS cthun-agent-bin DESTINATION bin)
