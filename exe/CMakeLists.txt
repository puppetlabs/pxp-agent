include_directories(
    ../lib/inc # the libpxp-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

add_executable(pxp-agent main.cc)
target_link_libraries(pxp-agent ${CPP_PCP_CLIENT_LIB} libpxp-agent)
install(TARGETS pxp-agent DESTINATION bin)

set(TASK_WRAPPER_LIBS ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "AIX")
    find_package(Threads)
    list(APPEND TASK_WRAPPER_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable(task_wrapper task_wrapper.cc)
target_link_libraries(task_wrapper ${TASK_WRAPPER_LIBS})
install(TARGETS task_wrapper DESTINATION bin)

set(TASK_LIBS ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "AIX")
    find_package(Threads)
    list(APPEND TASK_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable(task task.cc)
target_link_libraries(task ${TASK_LIBS})
install(TARGETS task DESTINATION ${MODULES_INSTALL_PATH})

add_executable(task-tests task.cc tests/main.cc)
target_compile_definitions(task-tests PRIVATE MAIN_IMPL=test_main)
target_link_libraries(task-tests ${TASK_LIBS})

add_test(
    NAME "task\\ tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/task-tests"
)
