cmake_minimum_required(VERSION 2.8.12)

set(PXP-AGENT_SOURCES
    main.cc
)

include_directories(
    ../lib/inc # the libpxp-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

set (PXP-AGENT_BIN_LIBS
    ${CPP_PCP_CLIENT_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    libpxp-agent
)

add_executable(pxp-agent ${PXP-AGENT_SOURCES})
target_link_libraries(pxp-agent ${PXP-AGENT_BIN_LIBS})

install(TARGETS pxp-agent DESTINATION bin)
