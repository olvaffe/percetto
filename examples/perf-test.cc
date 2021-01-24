/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>

#include <chrono>

#include "percetto.h"

PERCETTO_CATEGORY_DEFINE(test, "Test events", 0);

static bool trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories);
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

// Returns the duration per iteration in seconds.
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
}

int main(void) {
  const int enabled_event_count = 1000000;
  const int disabled_event_count = 100000000;

  if (!trace_init()) {
    fprintf(stderr, "failed to init tracing\n");
    return -1;
  }

  wait_for_tracing(false);

  run_perf_test("disabled", disabled_event_count);

  wait_for_tracing(true);

  run_perf_test("enabled", enabled_event_count);

  return 0;
}
