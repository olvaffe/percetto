# Copyright 2021-2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

add_executable(multi-category multi-category.c)
target_link_libraries(multi-category PRIVATE percetto::percetto)

add_executable(timestamps timestamps.c)
target_link_libraries(timestamps PRIVATE percetto::percetto)

add_executable(threads threads.cc)
target_link_libraries(threads PRIVATE percetto::percetto)

add_executable(perf-test perf-test.cc)
target_link_libraries(perf-test PRIVATE percetto::percetto)

add_library(multi-perfetto-shlib SHARED multi-perfetto-shlib.c)
target_link_libraries(multi-perfetto-shlib PUBLIC percetto::percetto)

add_executable(multi-perfetto-instance multi-perfetto-instance.c)
target_link_libraries(multi-perfetto-instance PRIVATE multi-perfetto-shlib)

if(ANDROID)
    add_executable(atrace atrace.cc atrace2.c)
    target_link_libraries(atrace PRIVATE percetto::atrace-percetto)
endif()
