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

percetto_lib = static_library(
  'percetto',
  'percetto.cc',
  'perfetto-port.cc',
  dependencies: perfetto_sdk_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os'],
  install : true,
)

percetto_dep = declare_dependency(
  include_directories: '.',
  link_with: percetto_lib,
)

atrace_lib = shared_library(
  'percetto-atrace',
  'percetto-atrace.c',
  dependencies: percetto_dep,
  c_args : ['-fvisibility=hidden', '-fdata-sections', '-ffunction-sections', '-Os'],
  link_args : ['-Wl,--gc-sections', '-Wl,--exclude-libs,ALL'],
  install : true,
)

atrace_dep = declare_dependency(
  include_directories: '.',
  link_with: atrace_lib,
)

install_headers('percetto.h', 'percetto-atrace.h')

pkg = import('pkgconfig')
pkg.generate(percetto_lib, description : 'C API wrapper library for perfetto')
pkg.generate(atrace_lib, description : 'Android ATRACE wrapper library for percetto')
