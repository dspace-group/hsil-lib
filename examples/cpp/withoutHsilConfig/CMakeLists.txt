# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

find_package(Threads REQUIRED)

set(targets
    hsilGroupedDataPubNoConfig
    hsilGroupedDataSubNoConfig
    hsilStreamingDataPubGenericNoConfig
    hsilStreamingDataSubGenericNoConfig
)

# Associate targets to their source files
add_executable(hsilGroupedDataPubNoConfig GroupedDataPub.cpp)
add_executable(hsilGroupedDataSubNoConfig GroupedDataSub.cpp)
add_executable(hsilStreamingDataPubGenericNoConfig StreamingDataGenericPub.cpp)
add_executable(hsilStreamingDataSubGenericNoConfig StreamingDataGenericSub.cpp)

# Using namespaced alias version (HsilCoSim::hsil_cosim) as it can be also be built standalone or as part of the hsil_cosim library
foreach(tgt ${targets})
    target_link_libraries(${tgt} PRIVATE HsilCoSim::hsil_cosim Threads::Threads)
    if(UNIX)
        target_link_libraries(${tgt} PRIVATE m)
    endif()

    # Make the example runnable straight from the build tree on Windows and from
    # the install tree on ELF/Mach-O platforms.
    hsil_copy_runtime_dlls(${tgt})
    hsil_set_install_rpath(${tgt})
endforeach()

install(TARGETS ${targets}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
