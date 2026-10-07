# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(SubprojectConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
include(CTest)

# Add HsilCoSim as subproject (pointing to the root of dspace-hsil-lib)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../.." "${CMAKE_CURRENT_BINARY_DIR}/hsil_cosim_subproject")

# Helper module for runtime DLL copying on Windows
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake")
include(HsilRuntimeDeps)

add_executable(subprojectConsumer main.c)

# Link against the namespaced target exported by HsilCoSim
target_link_libraries(subprojectConsumer PRIVATE HsilCoSim::hsil_cosim)

# Copy runtime DLLs next to executable on Windows
hsil_copy_runtime_dlls(subprojectConsumer)

add_test(NAME SubprojectConsumerTest COMMAND subprojectConsumer)
