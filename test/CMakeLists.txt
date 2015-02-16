# allow include directives with absolute paths for cthun-agent sources
set(BASEPATH "${CMAKE_CURRENT_LIST_DIR}/..")
include_directories("${BASEPATH}")
set(CATCH_DIRECTORY "${VENDOR_DIRECTORY}/catch")

set(SOURCES
    main.cpp

    unit/agent_test.cpp
    unit/configuration_test.cpp
    unit/data_container_test.cpp
    unit/external_module_test.cpp
    unit/file_utils_test.cpp
    unit/message_test.cpp
    unit/message_serialization_test.cpp
    unit/module_test.cpp
    unit/thread_container_test.cpp

    unit/modules/inventory_test.cpp
    unit/modules/ping_test.cpp
    unit/modules/status_test.cpp
)

set(test_BIN cthun-agent-unittests)

include_directories(
    ${CATCH_DIRECTORY}
)

ADD_EXECUTABLE(${test_BIN} ${SOURCES})
TARGET_LINK_LIBRARIES(
    ${test_BIN}
    cthun-agent
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -pthread")
endif()

ADD_CUSTOM_TARGET(check
    ALL
    "${MAINFOLDER}/bin/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${SOURCES}
)

ADD_CUSTOM_TARGET(test
    "${MAINFOLDER}/bin/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${test_SRCS}
)
