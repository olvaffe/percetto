/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "trace-1.h"

atomic_uint_fast32_t trace_instance_mask;

static void trace_state_callback(int category,
                                 int instance,
                                 int state,
                                 void* callback_data) {
  assert(category == 0);
  assert(instance < PERCETTO_MAX_DATA_SOURCE_INSTANCES);
  assert(callback_data == NULL);

  switch (state) {
    case PERCETTO_CATEGORY_STATE_START:
      atomic_fetch_or(&trace_instance_mask, 1 << instance);
      break;
    case PERCETTO_CATEGORY_STATE_STOP:
      atomic_fetch_and(&trace_instance_mask, ~(1 << instance));
      break;
    default:
      assert(false);
      break;
  }
}

bool trace_init(void) {
  static const char* category = "trace-1";
  return percetto_init(1, &category, trace_state_callback, NULL);
}

static void test(void) {
  TRACE_FUNC();

  {
    TRACE_SCOPED("nested scope");

    trace_begin("manual");
    trace_end();
  }
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
    if (trace_is_enabled())
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
