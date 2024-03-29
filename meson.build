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

project(
  'percetto',
  ['c', 'cpp'],
  version: '0.1.6',
  license: 'Apache-2.0',
  meson_version: '>=0.54.0',
  default_options: ['buildtype=debug',
                    'warning_level=2',
                    'werror=true',
                    'c_std=c11',
                    'cpp_std=c++2a'],
)

dep_threads = dependency('threads')
dep_perfetto = dependency('perfetto', fallback: ['perfetto', 'dep_perfetto'])

if (meson.is_subproject() and meson.version().version_compare('<0.63.0'))
  # for some reason the default is not picked up when
  # this is a subproject (fixed in meson 0.63.0)
  add_project_arguments('-std=c++2a', language : 'cpp')
endif

subdir('src')
if (not meson.is_subproject())
  # no need for the examples when this is a sub project
  subdir('examples')
endif
