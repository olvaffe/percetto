# Copyright 2020 Google LLC
# SPDX-License-Identifier: MIT

percetto_lib = static_library(
  'percetto',
  'percetto.cc',
  dependencies: perfetto_sdk_dep,
)

percetto_dep = declare_dependency(
  include_directories: '.',
  link_with: percetto_lib,
)
