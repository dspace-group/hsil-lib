# CMake 3.21 is required for PROJECT_IS_TOP_LEVEL and the
# $<TARGET_RUNTIME_DLLS:...> generator expression, both of which are used to
# keep this project well-behaved when it is consumed via add_subdirectory().
cmake_minimum_required(VERSION 3.21)
project(HsilCoSim
    VERSION     1.0.0
    DESCRIPTION "HSIL Co-Simulation API — lightweight C library for HSIL Co-Simulation networks"
    LANGUAGES   C CXX)

# ---------------------------------------------------------------------------- #
# Language standards                                                           #
# ---------------------------------------------------------------------------- #

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD            11)
set(CMAKE_C_STANDARD_REQUIRED   ON)

# Output directories are scoped to this project so that all HsilCoSim artifacts
# (library, DDS vendor runtime, examples, and tests) are generated into its own
# build folder, even when consumed via add_subdirectory(), without polluting the
# parent scope.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

# Internal CMake helper modules (not part of the installed package).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(HsilRuntimeDeps)

# enable_testing() / include(CTest) must be called at the top-level so that
# CTest generates CTestTestfile.cmake in the root build directory.
enable_testing()
include(CTest)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------- #
# Options                                                                      #
# ---------------------------------------------------------------------------- #

# Select which DDS vendor implementation to build. Each vendor lives in its own
# subdirectory under lib/src/vendors/. Adding a new vendor requires only
# creating the subdirectory with a CMakeLists.txt and implementing the vendor
# header.
set(HSIL_DDS_VENDOR "cycloneDDS" CACHE STRING
    "DDS vendor backend to use. Supported values: cycloneDDS")
set_property(CACHE HSIL_DDS_VENDOR PROPERTY STRINGS cycloneDDS)

# Build hsil_cosim as static library. By default only shared library is built.
# If this option is ON, only static library will be built and not shared library.
option(HSIL_BUILD_STATIC_LIB "Build static library" OFF)

# Build the bundled usage examples (examples/). Default to ON for top-level,
# OFF when consumed as a subproject via add_subdirectory().
if(PROJECT_IS_TOP_LEVEL)
    set(HSIL_BUILD_EXAMPLES_DEFAULT ON)
else()
    set(HSIL_BUILD_EXAMPLES_DEFAULT OFF)
endif()
option(HSIL_BUILD_EXAMPLES "Build the HSIL CoSim API usage examples" ${HSIL_BUILD_EXAMPLES_DEFAULT})

# Build the unit and integration test suite (lib/tests/).
# Opt-in: developers enable it through the CMake presets, which keeps the test
# suite (and its Google Test dependency) out of downstream builds that consume
# HsilCoSim via add_subdirectory().
option(HSIL_BUILD_TESTING  "Build the HSIL unit and integration tests" OFF)

# Build the DDS backend integration tests; requires a working network stack.
# Only meaningful when HSIL_BUILD_TESTING is ON.
option(HSIL_BUILD_TESTING_DDS
    "Build the DDS backend integration tests (require a working network stack)" ON)

# Install the documentation. Default: OFF
option(HSIL_INSTALL_DOCS "Install the documentation" OFF)


# Build gMock alongside gTest (consumed by the test suite).
option(HSIL_GTEST_BUILD_GMOCK "Build gMock alongside gTest" OFF)

# Every object file that ends up in a shared library must be compiled as
# position-independent code (-fPIC on GCC/Clang).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Release build if no build type is specified via -DCMAKE_BUILD_TYPE for single-configuration generators (e.g. Makefiles, Ninja).
# For multi-configuration generators (e.g. Visual Studio), the build type is selected at build time and CMAKE_BUILD_TYPE is ignored.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()


# ---------------------------------------------------------------------------- #
# Library                                                                      #
# ---------------------------------------------------------------------------- #

add_subdirectory(lib)

# ---------------------------------------------------------------------------- #
# Examples (opt-in)                                                            #
# ---------------------------------------------------------------------------- #

if(HSIL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------- #
# Tests (opt-in)                                                               #
# ---------------------------------------------------------------------------- #

if(HSIL_BUILD_TESTING)
    add_subdirectory(lib/tests)
endif()

# ---------------------------------------------------------------------------- #
# Installation rules
# ---------------------------------------------------------------------------- #

# Always install hsil_cosim License
install(
        FILES
            LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )

# Install documentation
if (HSIL_INSTALL_DOCS)
    install(
        DIRECTORY
            docs/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
endif()

