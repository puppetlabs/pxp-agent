cmake_minimum_required(VERSION 2.8.12)

set(PXP-AGENT_SOURCES
    main.cc
)

include_directories(
    ../lib/inc # the libpxp-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

add_executable(pxp-agent ${PXP-AGENT_SOURCES})
set_target_properties(pxp-agent PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(pxp-agent ${CPP_PCP_CLIENT_LIB} libpxp-agent)

install(TARGETS pxp-agent DESTINATION bin)
