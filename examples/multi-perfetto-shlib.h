/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef MULTI_PERFETTO_SHLIB_H
#define MULTI_PERFETTO_SHLIB_H

#include <stdbool.h>

__attribute__((visibility("default"))) bool test_shlib_init(void);
__attribute__((visibility("default"))) void test_shlib_func(void);

#endif /* MULTI_PERFETTO_SHLIB_H */
