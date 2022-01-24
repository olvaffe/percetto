# Copyright 2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

include(FeatureSummary)
set_package_properties(
    Percetto PROPERTIES
    URL "https://github.com/olvaffe/percetto/"
    DESCRIPTION "A C wrapper around the C++ Perfetto tracing SDK.")
include("${CMAKE_CURRENT_LIST_DIR}/percettoTargets.cmake")
