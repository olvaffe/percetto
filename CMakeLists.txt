# Copyright 2021-2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.10)
project(
    percetto
    VERSION 0.1.6.0
    DESCRIPTION "A C wrapper for the C++ Perfetto tracing library.")

# Increment every time the ABI changes
set(PERCETTO_SONAME_VERSION 1)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    # we are the top level project
    option(BUILD_EXAMPLES "Should we build the examples?" ON)
else()
    set(BUILD_EXAMPLES OFF)
endif()

if(NOT PERFETTO_SDK_PATH)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../perfetto/sdk)
        set(PERFETTO_SDK_PATH
            ${CMAKE_CURRENT_SOURCE_DIR}/../perfetto/sdk
            CACHE PATH "The SDK directory of a recent Perfetto release.")
    else()
        message(FATAL_ERROR "Must set PERFETTO_SDK_PATH")
    endif()
endif()

# Basic config stuff
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
include(GNUInstallDirs)
find_package(Threads REQUIRED)

if(ANDROID)
    find_library(ANDROID_LOG log)

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    option(INSTALL_FOR_PREFAB "Should we install for a manually-assembled prefab layout?" OFF)
    if(INSTALL_FOR_PREFAB)
        include(PrefabHelper)
        setup_prefab()

        setup_prefab_module(percetto)
    endif()
endif()

# Perfetto SDK
add_library(perfetto OBJECT ${PERFETTO_SDK_PATH}/perfetto.cc ${PERFETTO_SDK_PATH}/perfetto.h)
target_link_libraries(perfetto PUBLIC Threads::Threads)
target_include_directories(perfetto PUBLIC $<BUILD_INTERFACE:${PERFETTO_SDK_PATH}>)
target_compile_options(perfetto PRIVATE -ffunction-sections -fdata-sections)

# PerCetto
add_subdirectory(src)

# Optional examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Version usable in both build and install dirs.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/percetto/percettoConfigVersion.cmake"
    VERSION ${percetto_VERSION}
    COMPATIBILITY AnyNewerVersion)
# This really minimal config file usable in both build and install dirs
configure_file(cmake/percettoConfig.cmake
               "${CMAKE_CURRENT_BINARY_DIR}/percetto/percettoConfig.cmake" COPYONLY)

# Exported targets usable only for build
export(
    EXPORT percetto
    FILE "${CMAKE_CURRENT_BINARY_DIR}/percetto/percettoTargets.cmake"
    NAMESPACE percetto::)

# Only install if we are the top level project
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    # Installed targets usable only for installed
    set(PACKAGE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/percetto)
    install(
        EXPORT percetto
        FILE percettoTargets.cmake
        NAMESPACE percetto
        DESTINATION ${PACKAGE_INSTALL_DIR})

    # Install the files we share with the build tree.
    install(
        FILES cmake/percettoConfig.cmake
              "${CMAKE_CURRENT_BINARY_DIR}/percetto/percettoConfigVersion.cmake"
        DESTINATION ${PACKAGE_INSTALL_DIR})
endif()
