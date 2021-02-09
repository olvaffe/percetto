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

#define MY_PERCETTO_CATEGORIES(C, G) \
  C(gfx, "Graphics events")

PERCETTO_CATEGORY_DEFINE(MY_PERCETTO_CATEGORIES);

PERCETTO_TRACK_DEFINE(gpu, PERCETTO_TRACK_EVENTS);

static int trace_init(void) {
  int ret;
  struct timespec ts; (void)ts;
  assert(clock_gettime(CLOCK_BOOTTIME, &ts) == 0);
  ret = PERCETTO_INIT(PERCETTO_CLOCK_BOOTTIME);
  if (ret != 0)
    return ret;
  return PERCETTO_REGISTER_TRACK(gpu);
}

static void test(void) {
  if (PERCETTO_CATEGORY_IS_ENABLED(gfx)) {
    // Fabricate some GPU-like timings that have been gathered up for tracing.
    struct timespec ts = {0};
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
  int ret = trace_init();
  if (ret != 0) {
    fprintf(stderr, "failed to init tracing: %d\n", ret);
    return -1;
  }

  for (;;) {
    TRACE_EVENT(gfx, "do_test_iteration");
    test();

    struct timespec t = {0, 10000000};
    nanosleep(&t, NULL);
  }

  return 0;
}
