# allow include directives with absolute paths for cthun-agent sources
set(BASEPATH "${CMAKE_CURRENT_LIST_DIR}/..")
include_directories("${BASEPATH}")

set(SOURCES
    main.cpp
    file_utils_test.cpp
)

set(test_BIN cthun-agent-unittests)

include_directories(
    ${GMOCK_INCLUDE_DIRS}
)

ADD_EXECUTABLE(${test_BIN} ${SOURCES})
TARGET_LINK_LIBRARIES(
    ${test_BIN}
    cthun-agent
    ${GMOCK_LIBRARIES}
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
    SOURCES ${SOURCES})

ADD_CUSTOM_TARGET(test
    "${MAINFOLDER}/bin/${test_BIN}"
    DEPENDS ${test_BIN}
    COMMENT "Executing unit tests..."
    VERBATIM
    SOURCES ${test_SRCS})
