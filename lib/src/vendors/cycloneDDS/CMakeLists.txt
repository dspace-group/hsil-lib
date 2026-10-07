# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

# ---------------------------------------------------------------------------- #
# CycloneDDS Vendor Backend                                                     #
#                                                                               #
# This file builds the CycloneDDS implementation of the HSIL DDS abstraction   #
# layer. It uses CycloneDDS from the third_party/cyclonedds git submodule,     #
# compiles the HSIL CoSim IDL via idlc, and exposes the resulting static        #
# library as the 'hsilDdsVendor' target expected by the parent CMakeLists.      #
# ---------------------------------------------------------------------------- #

# ---------------------------------------------------------------------------- #
# CycloneDDS (from git submodule)                                               #
# ---------------------------------------------------------------------------- #

# Namespaced proxy options
# -------------------------
# Third-party submodules share the flat CMake cache, so generic names like
# BUILD_TESTING or ENABLE_LTO can collide between libraries. The pattern used
# here is:
#
#   1. Expose an HSIL_CYCLONEDDS_* option as the user-facing control surface.
#   2. FORCE the corresponding CycloneDDS cache variable to that value.
#
# Users should pass -DHSIL_CYCLONEDDS_BUILD_TESTING=ON etc. and avoid touching
# unqualified third-party option names directly. FORCE is safe here because it
# forwards the resolved HSIL-prefixed value into CycloneDDS before its own
# option() calls run.
option(HSIL_CYCLONEDDS_BUILD_TESTING "Build CycloneDDS internal tests" OFF)
option(HSIL_CYCLONEDDS_ENABLE_LTO    "Enable LTO for CycloneDDS"       OFF)

set(BUILD_TESTING  ${HSIL_CYCLONEDDS_BUILD_TESTING} CACHE BOOL "CycloneDDS: build tests" FORCE)
set(BUILD_EXAMPLES OFF  CACHE BOOL "CycloneDDS: build examples" FORCE)
set(BUILD_IDLC     ON   CACHE BOOL "CycloneDDS: build idlc" FORCE)
set(ENABLE_SSL     OFF  CACHE BOOL "CycloneDDS: enable SSL" FORCE)
set(ENABLE_SECURITY OFF CACHE BOOL "CycloneDDS: enable security" FORCE)
set(BUILD_DDSPERF  OFF  CACHE BOOL "CycloneDDS: build ddsperf" FORCE)
set(BUILD_DOCS     OFF  CACHE BOOL "CycloneDDS: build documentation" FORCE)
set(ENABLE_LTO     ${HSIL_CYCLONEDDS_ENABLE_LTO} CACHE BOOL "CycloneDDS: enable LTO" FORCE)

# CycloneDDS's docs/CMakeLists.txt unconditionally installs its LICENSE and
# other files to ${CMAKE_INSTALL_DOCDIR}. That variable is inherited from the
# top-level project and would resolve to share/doc/HsilCoSim. Override it
# here so CycloneDDS files land under their own name, then restore ours.
set(CMAKE_INSTALL_DOCDIR "share/doc/CycloneDDS" CACHE STRING "" FORCE)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/cyclonedds
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/cyclonedds
)
set(CMAKE_INSTALL_DOCDIR "share/doc/${CMAKE_PROJECT_NAME}" CACHE STRING "" FORCE)

# ---------------------------------------------------------------------------- #
# Generate C type-support code from the HSIL CoSim IDL                         #
# ---------------------------------------------------------------------------- #

# idlc_generate is provided by CycloneDDS after add_subdirectory.
# It generates HSIL-CoSim.c and HSIL-CoSim.h into the current binary directory.
idlc_generate(
    TARGET  hsilCosimIdl
    FILES   ${CMAKE_CURRENT_SOURCE_DIR}/idl/HSIL-CoSim.idl
    WARNINGS no-implicit-extensibility
)

get_target_property(IDL_SOURCES hsilCosimIdl INTERFACE_SOURCES)
get_target_property(IDL_HEADERS hsilCosimIdl INTERFACE_INCLUDE_DIRECTORIES)


# ---------------------------------------------------------------------------- #
# Vendor backend - Add sources, includes and link against cyclonedds           #
# ---------------------------------------------------------------------------- #

# Vendor is not a separate library but an object library which will be referenced by the main library.
add_library(hsilDdsVendorObject OBJECT
    cycloneDdsProvider.cpp
)

target_sources(hsilDdsVendorObject
    PRIVATE
        ${IDL_SOURCES}  # Add generated IDL sources to the vendor object library
)

target_include_directories(hsilDdsVendorObject
    PRIVATE
        # Expose the vendor directory itself so cycloneDdsProvider.h is findable.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        # Access the internal ddsAbstraction.h and the public hsil/hsil.h.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../src>         #TODO: hacky relative paths for now...
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>
        $<BUILD_INTERFACE:${IDL_HEADERS}>  # Include directory for generated IDL headers
)

# Hide all symbols of the vendor object library. They should not be visible to the consumers.
# C_VISIBILITY_PRESET covers the IDL-generated .c files; CXX_VISIBILITY_PRESET covers
# the C++ vendor sources. Both are needed because idlc emits C, not C++.
set_target_properties(hsilDdsVendorObject PROPERTIES
    C_VISIBILITY_PRESET       hidden
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN YES
)

target_link_libraries(hsilDdsVendorObject
    PRIVATE
        CycloneDDS::ddsc   # CycloneDDS C API
)
