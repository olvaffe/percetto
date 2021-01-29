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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "percetto.h"
#include "multi-perfetto-shlib.h"

PERCETTO_CATEGORY_DEFINE(test, "Test events", 0);

static int trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories,
                       PERCETTO_CLOCK_DONT_CARE);
}

static void test(void) {
  TRACE_EVENT(test, "main_prog");
  static int64_t flow_id = 1;
  ++flow_id;
  TRACE_FLOW(test, "flow1", flow_id);
  test_shlib_func(flow_id);
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

  ret = test_shlib_init();
  if (ret != 0) {
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
