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

# copy and build Perfetto SDK

fs = import('fs')

perfetto_sdk_path = get_option('perfetto-sdk')
if perfetto_sdk_path == ''
  error('option perfetto-sdk is not set')
endif

perfetto_sdk_path = fs.expanduser(perfetto_sdk_path)
if not fs.is_absolute(perfetto_sdk_path)
  error(perfetto_sdk_path + ' is not absolute')
endif
if not fs.is_dir(perfetto_sdk_path)
  error(perfetto_sdk_path + ' is not a directory')
endif

message('Perfetto SDK: ' + perfetto_sdk_path)

perfetto_sdk_gen = custom_target(
  'perfetto-sdk',
  input: [
    perfetto_sdk_path / 'perfetto.cc',
    perfetto_sdk_path / 'perfetto.h',
  ],
  output: [
    'perfetto.cc',
    'perfetto.h',
  ],
  command: ['cp', '@INPUT@', '@OUTDIR@'],
)
perfetto_sdk_h = perfetto_sdk_gen[1]

perfetto_sdk_lib = static_library(
  'perfetto-sdk',
  perfetto_sdk_gen,
  dependencies: threads_dep,
  cpp_args : ['-fdata-sections', '-ffunction-sections', '-Os', '-fvisibility=hidden'],
)

perfetto_sdk_dep = declare_dependency(
  link_with: perfetto_sdk_lib,
  sources: perfetto_sdk_h,
)
