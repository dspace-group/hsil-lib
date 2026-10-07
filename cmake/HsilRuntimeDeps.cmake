# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

# Internal helper: runtime DLL deployment and RPATH configuration for examples and tests.
include_guard(GLOBAL)

# Copies transitive runtime DLLs next to <target> post-build on Windows.
# Falls back to a no-op ('cmake -E true') if the target has no DLL dependencies - (e.g. static library)
function(hsil_copy_runtime_dlls TARGET_NAME)
    if(NOT WIN32)
        return()
    endif()

    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "hsil_copy_runtime_dlls: '${TARGET_NAME}' is not a target")
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>"
            "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>>:$<TARGET_FILE_DIR:${TARGET_NAME}>>"
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "Copying runtime DLLs next to ${TARGET_NAME}"
    )
endfunction()

# Sets relative origin-based install RPATH for <target> on POSIX/macOS.
function(hsil_set_install_rpath TARGET_NAME)
    if(WIN32)
        return()
    endif()

    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "hsil_set_install_rpath: '${TARGET_NAME}' is not a target")
    endif()

    include(GNUInstallDirs)

    if(APPLE)
        set(rpath "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    else()
        set(rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        INSTALL_RPATH            "${rpath}"
        BUILD_WITH_INSTALL_RPATH OFF
    )
endfunction()

