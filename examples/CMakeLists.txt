# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)


# Build examples as own project for standalone build (when not built as part of the hsil_cosim library directly)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(HsilCoSimExamples VERSION 1.0.0 LANGUAGES C CXX)

    # The example sources require C++17 (std::optional, ...). When built as part
    # of the library this comes from the top-level project.
    set(CMAKE_CXX_STANDARD          17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_package(HsilCoSim REQUIRED)

    include(GNUInstallDirs)

    # hsil_copy_runtime_dlls() is an internal helper and is therefore not part of
    # the installed package; pick it up from the repository instead.
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
    include(HsilRuntimeDeps)
endif()

# ---------------------------------------------------------------------------- #
# C++ examples                                                                 #
#                                                                              #
# Python examples live under python/ and do not participate in this            #
# CMake build. See those directories for instructions.                         #
# ---------------------------------------------------------------------------- #

add_subdirectory(cpp)
