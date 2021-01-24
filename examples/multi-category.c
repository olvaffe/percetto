/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE  // for nanosleep

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "multi-category.h"

PERCETTO_CATEGORY_DEFINE(cat, "Cat events", 0);
PERCETTO_CATEGORY_DEFINE(dog, "Dog events", PERCETTO_CATEGORY_FLAG_SLOW);

static bool trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(cat),
      PERCETTO_CATEGORY_PTR(dog),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories);
}

static void test(void) {
  TRACE_EVENT(cat, __func__);
  TRACE_EVENT(dog, "test2");
  const char* test3 = "test3";
  TRACE_EVENT(dog, test3);
}

int main(void) {
  const int wait = 60;
  const int event_count = 100;
  int i;

  if (!trace_init()) {
    printf("failed to init tracing\n");
    return -1;
  }

  test();

  for (i = 0; i < wait; i++) {
    if (PERCETTO_CATEGORY_IS_ENABLED(cat) && PERCETTO_CATEGORY_IS_ENABLED(dog))
      break;
    sleep(1);
  }
  if (i == wait) {
    printf("timed out waiting for tracing\n");
    return -1;
  }

  // Run forever to test consecutive trace sessions.
  for (;;) {
    for (i = 0; i < event_count; i++)
      test();
    struct timespec t = {0, 10000000};
    nanosleep(&t, NULL);
  }

  return 0;
}
