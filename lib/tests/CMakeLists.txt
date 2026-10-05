# ---------------------------------------------------------------------------- #
# HSIL Unit Tests                                                               #
#                                                                               #
# Three test executables, each with an independent link line:                  #
#                                                                               #
#   hsilConfigTests   – config parser (no DDS)                                 #
#   hsilApiTests      – API layer with a linker-seam stub vtable (no DDS)      #
#   hsilDdsTests      – real CycloneDDS backend (brokerless loopback)          #
#                       Only built when HSIL_BUILD_TESTING_DDS is ON.          #
# ---------------------------------------------------------------------------- #

cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------- #
# Google Test (git submodule)                                                   #
# ---------------------------------------------------------------------------- #

# Namespaced proxy options – same pattern as the CycloneDDS options.
option(HSIL_GTEST_BUILD_GMOCK "Build gMock alongside gTest" OFF)
set(BUILD_GMOCK ${HSIL_GTEST_BUILD_GMOCK} CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

if(NOT TARGET GTest::gtest AND NOT TARGET gtest)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/googletest
        ${CMAKE_CURRENT_BINARY_DIR}/third_party/googletest
        EXCLUDE_FROM_ALL
    )
endif()

# ---------------------------------------------------------------------------- #
# Shared compile settings for all test targets                                  #
# ---------------------------------------------------------------------------- #

# Internal headers (ddsAbstraction.h, hsilConfig.h)
set(HSIL_INTERNAL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# Root of the tests source tree. Injected into every test executable as
# HSIL_TESTS_DIR so tests can locate fixtures relative to it, e.g.:
#   std::string(HSIL_TESTS_DIR) + "/config/fixtures/valid_minimal.json"
# We convert backslashes to forward slashes to ensure it compiles safely as a string on Windows.
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" HSIL_TESTS_DIR)

# ---------------------------------------------------------------------------- #
# Layer 1 – hsilConfigTests                                                     #
# ---------------------------------------------------------------------------- #

# This target compiles hsilConfig.cpp directly (not via the
# hsil_cosim static library)
add_executable(hsilConfigTests
    config/test_hsilConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hsilConfig.cpp
)

target_compile_definitions(hsilConfigTests PRIVATE
    HSIL_TESTS_DIR="${HSIL_TESTS_DIR}"
)

target_include_directories(hsilConfigTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include    # <hsil/hsil.h>
    ${HSIL_INTERNAL_INCLUDES}   # hsilConfig.h
)

target_link_libraries(hsilConfigTests PRIVATE
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME HsilConfigTests COMMAND hsilConfigTests)

# ---------------------------------------------------------------------------- #
# Layer 2 – hsilApiTests (stub vtable – no DDS)                                #
# ---------------------------------------------------------------------------- #

# This target compiles hsil.cpp and hsilConfig.cpp directly (not via the
# hsil_cosim static library) and links stub_dds_ops.cpp instead of
# hsilDdsVendor, so there is no dependency on CycloneDDS at all.

add_executable(hsilApiTests
    api/test_hsilApi.cpp
    api/stub_dds_ops.cpp
    # Source files from the main library – compiled again with the stub vtable
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hsil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hsilConfig.cpp
)

target_include_directories(hsilApiTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include    # <hsil/hsil.h>
    ${HSIL_INTERNAL_INCLUDES}      # ddsAbstraction.h, hsilConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/api
)

target_link_libraries(hsilApiTests PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gtest_main
)

target_compile_definitions(hsilApiTests PRIVATE
    HSIL_TESTS_DIR="${HSIL_TESTS_DIR}"
)

add_test(NAME HsilApiTests COMMAND hsilApiTests)

# ---------------------------------------------------------------------------- #
# Layer 3 – hsilDdsTests (real CycloneDDS – opt-in)                            #
# ---------------------------------------------------------------------------- #

if(HSIL_BUILD_TESTING_DDS)
    add_executable(hsilDdsTests
        dds/test_cycloneDdsProvider.cpp
    )

    if(HSIL_DDS_VENDOR STREQUAL "cycloneDDS")
        target_include_directories(hsilDdsTests PRIVATE
             ${CMAKE_CURRENT_SOURCE_DIR}/../include    # <hsil/hsil.h>
            ${HSIL_INTERNAL_INCLUDES}   # ddsAbstraction.h, hsilConfig.h
            # Expose the vendor directory so ddsAbstraction.h resolves the provider header
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/vendors/cycloneDDS
            # Generated HSIL-CoSim.h lives in the binary directory of the vendor target
            ${CMAKE_CURRENT_BINARY_DIR}/src/vendors/cycloneDDS
        )
    
    else()
        message(FATAL_ERROR "Unsupported HSIL_DDS_VENDOR: ${HSIL_DDS_VENDOR}")
    endif()
   

    target_link_libraries(hsilDdsTests PRIVATE
        hsilDdsVendorObject
        GTest::gtest
        GTest::gtest_main
    )

    target_compile_definitions(hsilDdsTests PRIVATE
        HSIL_TESTS_DIR="${HSIL_TESTS_DIR}"
    )

    # This is the only test executable with a shared runtime dependency (the DDS
    # vendor library); ensure ctest can launch it regardless of the output layout.
    hsil_copy_runtime_dlls(hsilDdsTests)

    add_test(NAME HsilDdsTests COMMAND hsilDdsTests)
endif()

# ---------------------------------------------------------------------------- #
# Layer 4 integration tests (lib/tests/integration/) are standalone projects   #
# tested post-build / post-install via CI to avoid circular dependencies and   #
# verify find_package() against the install tree.                              #
# ---------------------------------------------------------------------------- #
