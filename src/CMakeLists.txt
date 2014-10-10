cmake_minimum_required(VERSION 2.8.12)

# allow include directives with absolute paths
set(BASEPATH "${CMAKE_CURRENT_LIST_DIR}")
INCLUDE_DIRECTORIES("${BASEPATH}")

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/agent_endpoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/schemas.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/external_module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/inventory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent/modules/ping.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/file_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/string_utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/common/log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/endpoint.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/connection.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/websocket/connection_manager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
)

add_executable(cthun-agent ${SOURCES})

target_link_libraries(cthun-agent
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${Boost_LIBRARIES}
    facter
)

install(TARGETS cthun-agent DESTINATION .)
