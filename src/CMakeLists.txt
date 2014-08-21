cmake_minimum_required(VERSION 2.8.12)

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
)

add_executable(cthun-agent ${SOURCES})

target_link_libraries(cthun-agent
    ${Boost_LIBRARIES}
)

install(TARGETS cthun-agent DESTINATION .)
