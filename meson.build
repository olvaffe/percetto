# Copyright 2020 Google LLC
# SPDX-License-Identifier: MIT

project(
  'percetto',
  ['c', 'cpp'],
  version: '0.1',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['buildtype=debug',
                    'warning_level=2',
                    'werror=true',
                    'c_std=c11',
                    'cpp_std=c++11'],
)

threads_dep = dependency('threads')

subdir('perfetto-sdk')
subdir('src')
subdir('examples')
