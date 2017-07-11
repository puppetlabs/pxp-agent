include_directories(
    ../lib/inc # the libpxp-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

add_executable(pxp-agent main.cc)
target_link_libraries(pxp-agent ${CPP_PCP_CLIENT_LIB} libpxp-agent)
install(TARGETS pxp-agent DESTINATION bin)

add_executable(task task.cc)
target_link_libraries(task ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
install(TARGETS task DESTINATION ${MODULES_INSTALL_PATH})

add_executable(test_task task.cc tests/main.cc)
target_compile_definitions(test_task PRIVATE MAIN_IMPL=test_main)
target_link_libraries(test_task ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
add_test(
    NAME "task tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/test_task"
)
