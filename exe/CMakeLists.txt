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
    ${CTHUN_CLIENT_INCLUDE_DIRS}
)

set (CTHUN-AGENT_BIN_LIBS
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${PTHREADS}
    ${facter_LIBRARY}
    ${CTHUN_CLIENT_LIB}
    libcthun-agent
)

add_executable(cthun-agent ${CTHUN-AGENT_SOURCES})
target_link_libraries(cthun-agent ${CTHUN-AGENT_BIN_LIBS})

install(TARGETS cthun-agent DESTINATION bin)
