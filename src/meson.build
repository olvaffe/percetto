# Copyright 2021 Google LLC
# SPDX-License-Identifier: MIT

percetto_lib = static_library(
  'percetto',
  'percetto.cc',
  'perfetto-port.cc',
  dependencies: perfetto_sdk_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
)

percetto_dep = declare_dependency(
  include_directories: '.',
  link_with: percetto_lib,
)
