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

#define _GNU_SOURCE  // for nanosleep

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "multi-category.h"

PERCETTO_CATEGORY_DEFINE(cat, "Cat events", 0);
PERCETTO_CATEGORY_DEFINE(dog, "Dog events", PERCETTO_CATEGORY_FLAG_SLOW);

PERCETTO_TRACK_DEFINE(squirrels, 42);

static int trace_init(void) {
  int ret;
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(cat),
      PERCETTO_CATEGORY_PTR(dog),
  };
  ret = percetto_init(sizeof(categories) / sizeof(categories[0]),
                      categories);
  if (ret != 0)
    return ret;
  ret = percetto_register_track(PERCETTO_TRACK_PTR(squirrels));
  return ret;
}

static void test(void) {
  TRACE_EVENT(cat, __func__);
  TRACE_EVENT(dog, "test2");
  const char* test3 = "instant";
  TRACE_INSTANT(dog, test3);
  static int count = 1;
  count++;
  TRACE_COUNTER(dog, squirrels, count);
}

int main(void) {
  const int wait = 60;
  const int event_count = 100;
  int i;
  int ret;

  ret = trace_init();
  if (ret != 0) {
    fprintf(stderr, "failed to init tracing: %d\n", ret);
    return -1;
  }

  test();

  for (i = 0; i < wait; i++) {
    if (PERCETTO_CATEGORY_IS_ENABLED(cat) || PERCETTO_CATEGORY_IS_ENABLED(dog))
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
