# Copyright 2021 Google LLC
# SPDX-License-Identifier: MIT

executable(
  'multi-category',
  'multi-category.c',
  dependencies: percetto_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)

executable(
  'size-bloat',
  'size-bloat.cc',
  dependencies: perfetto_sdk_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)
