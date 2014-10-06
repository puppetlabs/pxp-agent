cmake_minimum_required(VERSION 2.8.12)

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/schemas.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/external_module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/inventory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/ping.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/connection.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/connection_manager.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/endpoint.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/log/log.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/common/string_utils.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/common/file_utils.cpp"
)

add_executable(cthun-agent ${SOURCES})

target_link_libraries(cthun-agent
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${Boost_LIBRARIES}
    facter
)

install(TARGETS cthun-agent DESTINATION .)
