# Set include directories

include_directories(
    inc
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
    ${LEATHERMAN_CATCH_INCLUDE}
    ${CATCH_DIRECTORY}
)

set(COMMON_TEST_SOURCES
    main.cc
    unit/action_request_test.cc
    unit/agent_test.cc
    unit/certs.cc
    unit/configuration_test.cc
    unit/external_module_test.cc
    unit/request_processor_test.cc
    unit/module_test.cc
    unit/thread_container_test.cc
    unit/modules/ping_test.cc
    unit/modules/status_test.cc
)

if (UNIX)
    set(STANDARD_TEST_SOURCES
        unit/util/posix/pid_file_test.cc)
endif()

set(test_BIN pxp-agent-unittests)

set (PXP-AGENT_TEST_LIBS
    libpxp-agent
    ${cpp-pcp-client_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LEATHERMAN_LIBRARIES}
)

if (WIN32)
    set(PLATFORM_LIBS Ws2_32)
endif()

add_executable(${test_BIN} ${COMMON_TEST_SOURCES} ${STANDARD_TEST_SOURCES})
target_link_libraries(${test_BIN} ${PXP-AGENT_TEST_LIBS} ${PLATFORM_LIBS})

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
