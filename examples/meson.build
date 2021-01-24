# Copyright 2021 Google LLC
# SPDX-License-Identifier: MIT

executable(
  'multi-category',
  'multi-category.c',
  dependencies: percetto_dep,
  c_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)

executable(
  'size-bloat',
  'size-bloat.cc',
  dependencies: perfetto_sdk_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)

executable(
  'perf-test',
  'perf-test.cc',
  dependencies: percetto_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)

shlib = shared_library(
  'multi-perfetto-shlib',
  'multi-perfetto-shlib.c',
  dependencies: percetto_dep,
  c_args : ['-fvisibility=hidden', '-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections', '-Wl,--exclude-libs,ALL'],
  )

executable(
  'multi-perfetto-instance',
  'multi-perfetto-instance.c',
  dependencies: percetto_dep,
  c_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
  link_with : shlib,
)
