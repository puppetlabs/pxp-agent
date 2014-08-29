cmake_minimum_required(VERSION 2.8.12)

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/schemas.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/external_module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/client.cpp"
    "${VENDOR_DIRECTORY}/cthun-client/cthun-client/src/connection_metadata.cpp"
)

add_executable(cthun-agent ${SOURCES})

target_link_libraries(cthun-agent
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${Boost_LIBRARIES}
)

install(TARGETS cthun-agent DESTINATION .)
