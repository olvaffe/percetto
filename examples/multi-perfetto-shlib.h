/*
 * Copyright (C) 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MULTI_PERFETTO_SHLIB_H
#define MULTI_PERFETTO_SHLIB_H

#include <stdbool.h>

__attribute__((visibility("default"))) int test_shlib_init(void);
__attribute__((visibility("default"))) void test_shlib_func(void);

#endif /* MULTI_PERFETTO_SHLIB_H */
