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

#include <stdio.h>
#include <unistd.h>

#include <chrono>

#include "percetto.h"

PERCETTO_CATEGORY_DEFINE(test, "Test events");

static int trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories,
                       PERCETTO_CLOCK_DONT_CARE);
}

static void test(void) {
  TRACE_EVENT(test, __func__);
}

bool wait_for_tracing(bool is_enabled) {
  const int wait = 60;
  int i;
  for (i = 0; i < wait; i++) {
    if (PERCETTO_CATEGORY_IS_ENABLED(test) == is_enabled)
      break;
    sleep(1);
  }
  if (i == wait) {
    fprintf(stderr, "timed out waiting for tracing\n");
    return false;
  }
  return true;
}

void run_perf_test(const char* name, int iterations) {
  const double ns_per_sec = 1000000000.0;
  const double est_cycles_per_sec = 3000000000.0; // 3 GHz
  auto t1 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; i++)
    test();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = t2 - t1;
  double event_duration = elapsed.count() / static_cast<double>(iterations);
  fprintf(stderr, "%s: %f ns per TRACE_EVENT "
      "(est %.1f CPU cycles per begin/end)\n", name,
      event_duration * ns_per_sec,
      // Divide by 2 to account for begin + end events:
      event_duration * est_cycles_per_sec / 2.0);
  TRACE_DEBUG_DATA(test,
      PERCETTO_DOUBLE("ns per event", event_duration * ns_per_sec));
}

int main(void) {
  const int enabled_event_count = 1000000;
  const int disabled_event_count = 100000000;

  auto t1 = std::chrono::high_resolution_clock::now();
  int ret = trace_init();
  if (ret != 0) {
    fprintf(stderr, "failed to init tracing: %d\n", ret);
    return -1;
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = t2 - t1;
  fprintf(stderr, "percetto_init time: %f ms\n", elapsed.count() * 1000.0);

  wait_for_tracing(false);

  run_perf_test("disabled", disabled_event_count);

  wait_for_tracing(true);

  run_perf_test("enabled", enabled_event_count);

  return 0;
}
