cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------- #
# Latency Benchmark Example                                                     #
#                                                                               #
# Measures round-trip and estimated one-way DDS transport latency. Supports    #
# two modes of operation:                                                       #
#   • Single machine – Pinger and Ponger run in the same process on two         #
#     separate DDS participants communicating over the loopback interface.      #
#   • Two machines   – invoke with --role pinger or --role ponger to split      #
#     the roles across two hosts on the same DDS domain.                        #
# ---------------------------------------------------------------------------- #

find_package(Threads REQUIRED)

add_executable(hsilLatencyBenchmark latencyBenchmark.cpp)

# Using namespaced alias version (HsilCoSim::hsil_cosim) as it can be also be built standalone or as part of the hsil_cosim library
target_link_libraries(hsilLatencyBenchmark
    PRIVATE
        HsilCoSim::hsil_cosim
        Threads::Threads
)

# Make the example runnable straight from the build tree on Windows and from
# the install tree on ELF/Mach-O platforms.
hsil_copy_runtime_dlls(hsilLatencyBenchmark)
hsil_set_install_rpath(hsilLatencyBenchmark)


install(TARGETS hsilLatencyBenchmark
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
