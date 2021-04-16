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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for nanosleep
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "multi-category.h"

PERCETTO_CATEGORY_DEFINE(MY_PERCETTO_CATEGORIES);

PERCETTO_TRACK_DEFINE(squirrels, PERCETTO_TRACK_COUNTER);

static int trace_init(void) {
  int ret;
  ret = PERCETTO_INIT(PERCETTO_CLOCK_DONT_CARE);
  if (ret != 0)
    return ret;
  ret = PERCETTO_REGISTER_TRACK(squirrels);
  return ret;
}

static void test(void) {
  TRACE_EVENT(cat, __func__);
  TRACE_EVENT(dog, "test2");
  const char* test3 = "instant";
  (void)test3; // avoid unused warning when trace macros are disabled.
  TRACE_INSTANT(dog, test3);

  // Counter
  static int count = 1;
  count++;
  TRACE_COUNTER(dbg_or_dog, squirrels, count);

  // Debug annotations
  TRACE_DEBUG_DATA(dog, PERCETTO_BOOL("have_squirrels", count));
  TRACE_DEBUG_DATAS(dog, "my_data", PERCETTO_DOUBLE("inverse", 1.0 / count),
      PERCETTO_P(&count));
  TRACE_DEBUG_DATAS(dog, "more_data", PERCETTO_UINT("a", 2),
      PERCETTO_INT("b", -5), PERCETTO_STRING("str", "debug string"));
}

int main(void) {
  const int event_count = 100;
  int i;
  int ret;

  ret = trace_init();
  if (ret != 0) {
    fprintf(stderr, "failed to init tracing: %d\n", ret);
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
