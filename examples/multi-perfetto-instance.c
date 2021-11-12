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
#include <time.h>
#include <unistd.h>

#include "percetto.h"
#include "multi-perfetto-shlib.h"

#define MY_PERCETTO_CATEGORIES(C, G) \
  C(test, "Test events") \
  C(test2, "Slow test2 events", "slow")

PERCETTO_CATEGORY_DEFINE(MY_PERCETTO_CATEGORIES);

static int trace_init(void) {
  return PERCETTO_INIT(PERCETTO_CLOCK_DONT_CARE);
}

static void test(void) {
  TRACE_EVENT(test, "main_prog");
  static int64_t flow_id = 1;
  ++flow_id;
  TRACE_FLOW(test, "flow1", flow_id);
  TRACE_EVENT_DATA(test, "shlib call", PERCETTO_I(flow_id));
  test_shlib_func(flow_id);
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

  ret = test_shlib_init();
  if (ret != 0) {
    printf("failed to init shlib\n");
    return -1;
  }

  for (;;) {
    for (i = 0; i < event_count; i++)
      test();
    struct timespec t = {0, 10000000};
    nanosleep(&t, NULL);
  }

  return 0;
}
