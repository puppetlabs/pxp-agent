# Set include directories

include_directories(
    inc
    SYSTEM ${Boost_INCLUDE_DIRS}
    SYSTEM ${Boost_Process_INCLUDE_DIRS}
    SYSTEM ${HORSEWHISPERER_INCLUDE_DIRS}
    SYSTEM ${INIH_INCLUDE_DIRS}
    SYSTEM ${CTHUN_CLIENT_INCLUDE_DIRS}
    SYSTEM ${LEATHERMAN_CATCH_INCLUDE}
    SYSTEM ${CATCH_DIRECTORY}
)

set(SOURCES
    main.cc
    unit/agent_test.cc
    unit/certs.cc
    unit/configuration_test.cc
    unit/external_module_test.cc
    unit/file_utils_test.cc
    unit/module_test.cc
    unit/thread_container_test.cc
    unit/modules/inventory_test.cc
    unit/modules/ping_test.cc
    unit/modules/status_test.cc
)

set(test_BIN cthun-agent-unittests)

set (CTHUN-AGENT_TEST_LIBS
    ${CTHUN_CLIENT_LIB}
    libcthun-agent
)

add_executable(${test_BIN} ${SOURCES})
target_link_libraries(${test_BIN} ${CTHUN-AGENT_TEST_LIBS})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -pthread")
endif()

ADD_CUSTOM_TARGET(check
    "${MAINFOLDER}/bin/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${SOURCES}
)
