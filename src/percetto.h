/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef PERCETTO_H
#define PERCETTO_H

#include <stdint.h>

/* must be <=32 because instance_mask is uint32_t */
#define PERCETTO_MAX_DATA_SOURCE_INSTANCES 8

#ifdef __cplusplus
extern "C" {
#endif

enum percetto_category_state {
  PERCETTO_CATEGORY_STATE_START,
  PERCETTO_CATEGORY_STATE_STOP,
};

/* this may be called anytime by any thread */
typedef void (*percetto_category_state_callback)(int category,
                                                 int instance,
                                                 int state,
                                                 void* callback_data);

bool percetto_init(int category_count,
                   const char** categories,
                   percetto_category_state_callback callback,
                   void* callback_data);

void percetto_slice_begin(int category,
                          uint32_t instance_mask,
                          const char* name);

void percetto_slice_end(int category, uint32_t instance_mask);

void percetto_instant(int category, uint32_t instance_mask, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* PERCETTO_H */
