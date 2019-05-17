# Set include directories

include_directories(
    inc
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
    ${LEATHERMAN_CATCH_INCLUDE}
)

set(COMMON_TEST_SOURCES
    main.cc
    common/certs.cc
    common/mock_connector.cc
    component/external_modules_interface_test.cc
    unit/action_request_test.cc
    unit/action_response_test.cc
    unit/agent_test.cc
    unit/configuration_test.cc
    unit/external_module_test.cc
    unit/module_test.cc
    unit/pxp_connector_v1_test.cc
    unit/pxp_connector_v2_test.cc
    unit/request_processor_test.cc
    unit/results_mutex_test.cc
    unit/results_storage_test.cc
    unit/thread_container_test.cc
    unit/time_test.cc
    unit/modules/command_test.cc
    unit/modules/ping_test.cc
    unit/modules/task_test.cc
    unit/util/process_test.cc
)

if (UNIX)
    set(STANDARD_TEST_SOURCES
        unit/util/posix/pid_file_test.cc)
endif()

set(test_BIN pxp-agent-unittests)

add_executable(${test_BIN} ${COMMON_TEST_SOURCES} ${STANDARD_TEST_SOURCES})
target_link_libraries(${test_BIN} libpxp-agent)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -pthread")
endif()

ADD_CUSTOM_TARGET(check
    "${EXECUTABLE_OUTPUT_PATH}/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${SOURCES}
)
