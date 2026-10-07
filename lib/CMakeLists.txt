# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

# ---------------------------------------------------------------------------- #
# Dependencies (git submodules)                                                #
# ---------------------------------------------------------------------------- #

# Both dependencies live as git submodules under third_party/ at the repository
# root.  Initialise them with:
#   git submodule update --init --recursive
#
# nlohmann JSON – header-only, used for simulation configuration parsing.
if(NOT TARGET nlohmann_json::nlohmann_json AND NOT TARGET nlohmann_json)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann_json
        ${CMAKE_CURRENT_BINARY_DIR}/third_party/nlohmann_json
    )
endif()

# ---------------------------------------------------------------------------- #
# Vendor backend                                                               #
# ---------------------------------------------------------------------------- #

# Each vendor subdirectory must define an OBJECT library target
# named "hsilDdsVendorObject" that the main library links against
add_subdirectory(src/vendors/${HSIL_DDS_VENDOR})

# ---------------------------------------------------------------------------- #
# Main library (shared (by default) or static if HSIL_BUILD_STATIC_LIB is ON)  #
# ---------------------------------------------------------------------------- #

if (NOT HSIL_BUILD_STATIC_LIB)
    add_library(hsil_cosim SHARED
        src/hsil.cpp
        src/hsilConfig.cpp
    )
else()
    add_library(hsil_cosim STATIC
        src/hsil.cpp
        src/hsilConfig.cpp
    )
endif()

# Add namespaced version as alias. Users building this library link against either hsil_cosim or HsilCoSim::hsil_cosim.
add_library(HsilCoSim::hsil_cosim ALIAS hsil_cosim)

# Look for shared library dependencies in the same directory as the main library
set_target_properties(hsil_cosim PROPERTIES
    OUTPUT_NAME  hsil_cosim
    INSTALL_RPATH "$ORIGIN"
)

target_include_directories(hsil_cosim
    PUBLIC
        # Consumers of the installed library use this include path.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        # Internal headers (ddsAbstraction.h, hsilConfig.h) live here.
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)

target_link_libraries(hsil_cosim
    PUBLIC
        # Linking against object library and propagating its dependency to cyclonedds library to the consumers of the API.
        # Allowed in cmake version > 3.12
        hsilDdsVendorObject
        
        Threads::Threads
    PRIVATE
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# When building statically on non-MSVC compilers, downstream pure C consumers
# need the C++ standard library linked transitively.
if (HSIL_BUILD_STATIC_LIB AND NOT MSVC)
    target_link_libraries(hsil_cosim PUBLIC $<LINK_ONLY:stdc++>)
endif()

if (NOT HSIL_BUILD_STATIC_LIB)
    # Activate dllexport on Windows; on ELF platforms this triggers
    # __attribute__((visibility("default"))) for every HSIL_API symbol.
    target_compile_definitions(hsil_cosim PRIVATE HSIL_BUILDING_SHARED)

    # Hide all symbols except those explicitly marked HSIL_API.
    # CMake translates these properties to the correct compiler flags on each
    # platform (-fvisibility=hidden on GCC/Clang, no-op on MSVC which uses
    # __declspec instead).
    set_target_properties(hsil_cosim PROPERTIES
        C_VISIBILITY_PRESET       hidden
        CXX_VISIBILITY_PRESET     hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
endif()

# ---------------------------------------------------------------------------- #
# Installation rules                                                           #
# ---------------------------------------------------------------------------- #

include(GNUInstallDirs)

# install hsilDdsVendorObject for propagating the link dependency to the DDS vendor library to the consumers of the API.
install(TARGETS hsilDdsVendorObject
        EXPORT HsilCoSimTargets
)

install(TARGETS hsil_cosim
        EXPORT HsilCoSimTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}   # .lib import lib on Windows
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}   # .so on Linux / macOS
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}   # .dll on Windows
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the target information for find_package and CMake config mode usage.
install(EXPORT HsilCoSimTargets
    FILE HsilCoSimTargets.cmake
    NAMESPACE HsilCoSim::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HsilCoSim
)

# Install nlohmann_json License (since  it being used by the libraray being installed)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann_json/LICENSE.MIT
        DESTINATION share/doc/nlohmann_json
)

# CycloneDDS license is automatically installed (as it is added as a sub project) when the install rule is run

# Use a standard helper module for packaging
include(CMakePackageConfigHelpers) 

# Generate the main Config file from our template
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HsilCoSimConfig.cmake.in"
  "HsilCoSimConfig.cmake"
  INSTALL_DESTINATION
  "${CMAKE_INSTALL_LIBDIR}/cmake/HsilCoSim"
)

# Generate a version file for version checking
write_basic_package_version_file( 
  "HsilCoSimConfigVersion.cmake" 
  VERSION ${PROJECT_VERSION} 
  COMPATIBILITY AnyNewerVersion 
)

# Install the generated config and version files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HsilCoSimConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HsilCoSimConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HsilCoSim
)
