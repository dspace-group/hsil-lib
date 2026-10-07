# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

# ---------------------------------------------------------------------------- #
# C++ Examples                                                                  #
#                                                                               #
# All three examples link against hsil_cosim which is defined in the parent   #
# CMake scope (either from the lib/ super-project or via find_package).         #
# ---------------------------------------------------------------------------- #

add_subdirectory(bmsScenarioWithHsilConfig)
add_subdirectory(latencyBenchmark)
add_subdirectory(withoutHsilConfig)
