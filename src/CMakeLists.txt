# Copyright 2021-2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

# PerCetto
add_library(percetto SHARED percetto.cc perfetto-port.cc $<TARGET_OBJECTS:perfetto>)
target_compile_options(percetto PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(percetto PUBLIC Threads::Threads)
target_link_options(percetto PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
set_target_properties(
    percetto
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden
        SOVERSION ${PERCETTO_SONAME_VERSION}
        VERSION ${PERCETTO_SONAME_VERSION})
target_include_directories(percetto PUBLIC $<BUILD_INTERFACE:${PERFETTO_SDK_PATH}>)
target_include_directories(percetto PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES percetto.h perfetto-port.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    TARGETS percetto
    EXPORT percetto
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(ANDROID)
    target_link_libraries(percetto PRIVATE ${ANDROID_LOG})
endif()

# Provide alias so that examples can work the same as out-of-project usage.
add_library(percetto::percetto ALIAS percetto)

if(ANDROID)
    # Percetto ATRACE wrapper

    # Warning: PrefabPackageTask.findLibraryForAbi only looks if a file "starts with"
    # a given name, so libraries must not be prefixes of each other.
    # That is why this is called atrace-percetto and not percetto-atrace.
    # The error is very cryptic, mentioning more than one item in a container.
    # see https://android.googlesource.com/platform/tools/base/+/d6150e09027e8248d2a1c801b609f0a1db9a297e/build-system/gradle-core/main/java/com/android/build/gradle/internal/tasks/PrefabPublishing.kt#246
    add_library(atrace-percetto SHARED percetto-atrace.c)
    target_link_libraries(atrace-percetto PUBLIC percetto Threads::Threads)
    set_target_properties(atrace-percetto PROPERTIES CXX_VISIBILITY_PRESET hidden
                                                     C_VISIBILITY_PRESET hidden)
    target_link_options(atrace-percetto PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)

    # Provide alias so that examples can work the same as out-of-project usage.
    add_library(percetto::atrace-percetto ALIAS atrace-percetto)

    # Installing
    install(FILES percetto-atrace.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(
        TARGETS atrace-percetto
        EXPORT percetto
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if(INSTALL_FOR_PREFAB)
        install(FILES AndroidManifest.xml DESTINATION .)
        # This gets used directly by build-aar.sh
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pom
                       ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}.pom)
    endif()
endif()
