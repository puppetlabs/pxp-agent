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

add_executable(task_wrapper-tests tests/main.cc)
target_link_libraries(task_wrapper-tests ${TASK_WRAPPER_LIBS})

# Copy powershell shim to bin so we can use with local testing.
set(POWERSHELL_SHIM ${CMAKE_BINARY_DIR}/bin/powershell_shim.ps1)
add_custom_command(
    OUTPUT ${POWERSHELL_SHIM}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/powershell_shim.ps1 ${POWERSHELL_SHIM}
    DEPENDS powershell_shim.ps1)
add_custom_target(powershell_shim ALL DEPENDS ${POWERSHELL_SHIM})
install(FILES powershell_shim.ps1 DESTINATION bin
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

add_test(
    NAME "task\\ wrapper\\ tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/task_wrapper-tests"
)
