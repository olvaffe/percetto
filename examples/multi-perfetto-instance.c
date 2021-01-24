/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "percetto.h"
#include "multi-perfetto-shlib.h"

PERCETTO_CATEGORY_DEFINE(test, "Test events", 0);

static bool trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories);
}

static void test(void) {
  TRACE_EVENT(test, "main_prog");
  test_shlib_func();
}

int main(void) {
  const int wait = 60;
  const int event_count = 100;
  int i;

  if (!trace_init()) {
    printf("failed to init tracing\n");
    return -1;
  }

  if (!test_shlib_init()) {
    printf("failed to init shlib\n");
    return -1;
  }

  test();

  for (i = 0; i < wait; i++) {
    if (PERCETTO_CATEGORY_IS_ENABLED(test))
      break;
    sleep(1);
  }
  if (i == wait) {
    printf("timed out waiting for tracing\n");
    return -1;
  }

  for (i = 0; i < event_count; i++)
    test();

  return 0;
}
