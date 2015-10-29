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

set (PXP-AGENT_BIN_LIBS
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

add_executable(pxp-agent ${PXP-AGENT_SOURCES})
target_link_libraries(pxp-agent ${PXP-AGENT_BIN_LIBS} ${PLATFORM_LIBS})

install(TARGETS pxp-agent DESTINATION bin)
