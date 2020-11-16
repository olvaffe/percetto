/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "multi-category.h"

atomic_uint_fast32_t trace_instance_masks[TRACE_COUNT];

static void trace_state_callback(int category,
                                 int instance,
                                 int state,
                                 void* callback_data) {
  assert(category < TRACE_COUNT);
  assert(instance < PERCETTO_MAX_DATA_SOURCE_INSTANCES);
  assert(callback_data == NULL);

  switch (state) {
    case PERCETTO_CATEGORY_STATE_START:
      atomic_fetch_or(&trace_instance_masks[category], 1 << instance);
      break;
    case PERCETTO_CATEGORY_STATE_STOP:
      atomic_fetch_and(&trace_instance_masks[category], ~(1 << instance));
      break;
    default:
      assert(false);
      break;
  }
}

bool trace_init(void) {
  static const char* categories[TRACE_COUNT] = {
      [TRACE_CAT] = "cat",
      [TRACE_DOG] = "dog",
  };
  return percetto_init(TRACE_COUNT, categories, trace_state_callback, NULL);
}

static void test(void) {
  trace_begin(TRACE_CAT, "cat");
  trace_begin(TRACE_DOG, "dog");
  trace_end(TRACE_DOG);
  trace_end(TRACE_CAT);
}

int main(void) {
  const int wait = 5;
  int i;

  if (!trace_init()) {
    printf("failed to init tracing\n");
    return -1;
  }

  test();

  for (i = 0; i < wait; i++) {
    int j;
    for (j = 0; j < TRACE_COUNT; j++) {
      if (!trace_is_enabled(j))
        break;
    }
    if (j == TRACE_COUNT)
      break;
    sleep(1);
  }
  if (i == wait) {
    printf("timed out waiting for tracing\n");
    return -1;
  }

  test();
  /* flush? */

  return 0;
}
