cmake_minimum_required(VERSION 3.21)

find_package(Threads REQUIRED)

function(add_hsil_app TRGT SRC CONFIG)
    # Create an executable target
    add_executable(${TRGT} ${SRC})
    
    # Link libraries to the target 
    # Using namespaced alias version (HsilCoSim::hsil_cosim) as it can be also be built standalone or as part of the hsil_cosim library
    target_link_libraries(${TRGT} PRIVATE HsilCoSim::hsil_cosim Threads::Threads)
    if(UNIX)
        target_link_libraries(${TRGT} PRIVATE m)
    endif()

    # Make the example runnable straight from the build tree on Windows and from
    # the install tree on ELF/Mach-O platforms.
    hsil_copy_runtime_dlls(${TRGT})
    hsil_set_install_rpath(${TRGT})

    # Copy the config file to the same directory as the executable after build
    add_custom_command(TARGET ${TRGT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG}
            $<TARGET_FILE_DIR:${TRGT}>/${CONFIG}
    )

    # Installation rules
    install(TARGETS ${TRGT}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

set(targets
    hsilTempSensor
    hsilBmsEcu
    hsilCoolingEcu
)

add_hsil_app(hsilTempSensor tempSensor.cpp sensorConfig.json)
add_hsil_app(hsilBmsEcu bmsEcu.cpp bmsConfig.json)
add_hsil_app(hsilCoolingEcu coolingEcu.cpp coolingConfig.json)