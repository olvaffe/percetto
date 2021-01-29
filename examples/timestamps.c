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
#include <time.h>
#include <unistd.h>

#include "percetto.h"

PERCETTO_CATEGORY_DEFINE(gfx, "Graphics events", 0);

PERCETTO_TRACK_DEFINE(gpu, PERCETTO_TRACK_NORMAL, 13);

static int trace_init(void) {
  int ret;
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(gfx),
  };
  struct timespec ts; (void)ts;
  assert(clock_gettime(CLOCK_BOOTTIME, &ts) == 0);
  ret = percetto_init(sizeof(categories) / sizeof(categories[0]), categories,
                      PERCETTO_CLOCK_BOOTTIME);
  if (ret != 0)
    return ret;
  return percetto_register_track(PERCETTO_TRACK_PTR(gpu));
}

static void test(void) {
  if (PERCETTO_CATEGORY_IS_ENABLED(gfx)) {
    // Fabricate some GPU-like timings that have been gathered up for tracing.
    struct timespec ts = {};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    uint64_t ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    // Move timestamp back by 16 ms to when the fake GPU events began.
    ns -= 16 * 1000000;

    TRACE_EVENT_BEGIN_ON_TRACK(gfx, gpu, ns, "GPU"); ns += 1000;
      TRACE_EVENT_BEGIN_ON_TRACK(gfx, gpu, ns, "draw1"); ns += 1000000;
      TRACE_EVENT_END_ON_TRACK(gfx, gpu, ns); ns += 2000;
      TRACE_EVENT_BEGIN_ON_TRACK(gfx, gpu, ns, "draw2"); ns += 2000000;
      TRACE_EVENT_END_ON_TRACK(gfx, gpu, ns); ns += 1000;
    TRACE_EVENT_END_ON_TRACK(gfx, gpu, ns); ns += 1000;
  }
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

  for (i = 0; i < wait; i++) {
    if (PERCETTO_CATEGORY_IS_ENABLED(gfx))
      break;
    sleep(1);
  }
  if (i == wait) {
    printf("timed out waiting for tracing\n");
    return -1;
  }

  for (i = 0; i < event_count; i++) {
    TRACE_EVENT(gfx, "do_test_iteration");
    test();

    struct timespec t = {0, 50000000};
    nanosleep(&t, NULL);
  }

  return 0;
}
