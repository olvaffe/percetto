# Copyright (C) 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

executable(
  'multi-category',
  'multi-category.c',
  dependencies: percetto_dep,
  c_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections'],
)

executable(
  'timestamps',
  'timestamps.c',
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
