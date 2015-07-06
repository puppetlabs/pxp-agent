cmake_minimum_required(VERSION 2.8.12)

set(CTHUN-AGENT_SOURCES
    main.cc
)

include_directories(
    ../lib/inc # the libcthun-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${INIH_INCLUDE_DIRS}
    ${cthun-client_INCLUDE_DIR}
)

set (CTHUN-AGENT_BIN_LIBS
    ${CMAKE_THREAD_LIBS_INIT}
    ${cthun-client_LIBRARY}
    libcthun-agent
)

add_executable(cthun-agent ${CTHUN-AGENT_SOURCES})
target_link_libraries(cthun-agent ${CTHUN-AGENT_BIN_LIBS})

install(TARGETS cthun-agent DESTINATION bin)
