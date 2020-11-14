/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef TRACE_2_H
#define TRACE_2_H

#include <stdatomic.h>
#include <stdbool.h>

#include "percetto.h"

enum trace_category {
  TRACE_CAT,
  TRACE_DOG,

  TRACE_COUNT,
};

extern atomic_uint_fast32_t trace_instance_masks[TRACE_COUNT];

bool trace_init(void);

static inline bool trace_is_enabled(enum trace_category category) {
  return trace_instance_masks[category];
}

static inline void trace_begin(enum trace_category category, const char* name) {
  const uint32_t mask = atomic_load(&trace_instance_masks[category]);
  if (mask)
    percetto_slice_begin(0, mask, name);
}

static inline void trace_end(enum trace_category category) {
  const uint32_t mask = atomic_load(&trace_instance_masks[category]);
  if (mask)
    percetto_slice_end(category, mask);
}

#endif /* TRACE_2_H */
