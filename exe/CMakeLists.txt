include_directories(
    ../lib/inc # the libpxp-agent headers
    ${LEATHERMAN_INCLUDE_DIRS}
    ${HORSEWHISPERER_INCLUDE_DIRS}
    ${cpp-pcp-client_INCLUDE_DIR}
)

add_executable(pxp-agent main.cc)
target_link_libraries(pxp-agent libpxp-agent)
install(TARGETS pxp-agent DESTINATION bin)

install(FILES apply_ruby_shim.rb DESTINATION bin
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(EXECUTION_WRAPPER_LIBS ${Boost_LIBRARIES} ${LEATHERMAN_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "AIX")
    find_package(Threads)
    list(APPEND EXECUTION_WRAPPER_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable(execution_wrapper execution_wrapper.cc)
target_link_libraries(execution_wrapper ${EXECUTION_WRAPPER_LIBS})
install(TARGETS execution_wrapper DESTINATION bin)

include_directories(${LEATHERMAN_CATCH_INCLUDE})
add_executable(execution_wrapper-tests tests/main.cc)
target_link_libraries(execution_wrapper-tests ${EXECUTION_WRAPPER_LIBS})

# Copy powershell shim to bin so we can use with local testing.
foreach(shim PowershellShim.ps1 PowershellShim-Helper.ps1)
    set(shim_local ${CMAKE_BINARY_DIR}/bin/${shim})
    add_custom_command(
        OUTPUT ${shim_local}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${shim} ${shim_local}
        DEPENDS ${shim})
    add_custom_target(${shim} ALL DEPENDS ${shim_local})
    install(FILES ${shim} DESTINATION bin
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach(shim)

add_test(
    NAME "task\\ wrapper\\ tests"
    COMMAND "${EXECUTABLE_OUTPUT_PATH}/execution_wrapper-tests"
)
