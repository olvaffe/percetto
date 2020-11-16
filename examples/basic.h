/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef BASIC_H
#define BASIC_H

#include <stdatomic.h>
#include <stdbool.h>

#include "percetto.h"

extern atomic_uint_fast32_t trace_instance_mask;

bool trace_init(void);

static inline bool trace_is_enabled(void) {
  return atomic_load(&trace_instance_mask);
}

static inline void trace_begin(const char* name) {
  const uint32_t mask = atomic_load(&trace_instance_mask);
  if (mask)
    percetto_slice_begin(0, mask, name);
}

static inline void trace_end(void) {
  const uint32_t mask = atomic_load(&trace_instance_mask);
  if (mask)
    percetto_slice_end(0, mask);
}

static inline const void* trace_begin_scoped(const char* name) {
  trace_begin(name);
  return NULL;
}

static inline void trace_end_scoped(const void** name) {
  (void)name;
  trace_end();
}

#define TRACE_SCOPED(name)                                 \
  const void* _trace_scoped##__LINE__                      \
      __attribute__((cleanup(trace_end_scoped), unused)) = \
          trace_begin_scoped(name)

#define TRACE_FUNC() TRACE_SCOPED(__func__)

#endif /* BASIC_H */
