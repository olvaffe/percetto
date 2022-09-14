# Copyright 2020-2021, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0
#
# Helpers for creating an android "prefab" archive

# We must run the following at "include" time, not at function call time,
# to find the path to this module rather than the path to a calling list file
get_filename_component(_prefab_mod_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

macro(setup_prefab)
    set(PREFAB_INSTALL_DIR prefab)
    unset(NDK_MAJOR_VERSION)
    if(CMAKE_ANDROID_NDK)
        file(STRINGS "${CMAKE_ANDROID_NDK}/source.properties" NDK_PROPERTIES)
        foreach(_line ${NDK_PROPERTIES})
            if("${_line}" MATCHES
               "Pkg.Revision = ([0-9]+)[.]([0-9]+)[.]([0-9]+)")
                set(NDK_MAJOR_VERSION ${CMAKE_MATCH_1})
            endif()
        endforeach()
    else()
        message(FATAL_ERROR "Please set CMAKE_ANDROID_NDK to your NDK root!")
    endif()
    if(NDK_MAJOR_VERSION)
        message(STATUS "Building using NDK major version ${NDK_MAJOR_VERSION}")
    else()
        message(
            FATAL_ERROR
                "Could not parse the major version from ${CMAKE_ANDROID_NDK}/source.properties"
        )
    endif()

    configure_file(${_prefab_mod_dir}/prefab.json
                   ${CMAKE_CURRENT_BINARY_DIR}/prefab.json @ONLY)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/prefab.json
        DESTINATION ${PREFAB_INSTALL_DIR}
        COMPONENT Prefab)
endmacro()

macro(setup_prefab_module MODULE_NAME)
    set(PREFAB_MODULE_INSTALL_DIR ${PREFAB_INSTALL_DIR}/modules/${MODULE_NAME})
    set(CMAKE_INSTALL_LIBDIR
        ${PREFAB_MODULE_INSTALL_DIR}/libs/android.${ANDROID_ABI})
    set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_INSTALL_INCLUDEDIR ${PREFAB_MODULE_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})

    configure_file(${_prefab_mod_dir}/abi.json
                   ${CMAKE_CURRENT_BINARY_DIR}/abi.json @ONLY)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/abi.json
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT Prefab)

    install(
        FILES ${_prefab_mod_dir}/module.json
        DESTINATION ${PREFAB_MODULE_INSTALL_DIR}
        COMPONENT Prefab)
endmacro()
