set(test_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/agent_test.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/modules/echo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/jsoncpp/jsoncpp.cpp"
)

set(test_BIN cthun-agent-unittests)

include_directories(
    ${GMOCK_INCLUDE_DIRS}
)

add_executable(${test_BIN} ${test_SOURCE})
target_link_libraries(${test_BIN}
   ${GMOCK_LIBRARIES}
   ${Boost_LIBRARIES}
)

ADD_CUSTOM_TARGET(check
    ALL
    "${EXECUTABLE_OUTPUT_PATH}/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${test_SRCS})
