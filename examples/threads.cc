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

#include <thread>
#include <vector>

#include "percetto.h"

#define MY_PERCETTO_CATEGORIES(C, G) \
  C(test, "Test events")

PERCETTO_CATEGORY_DEFINE(MY_PERCETTO_CATEGORIES);

PERCETTO_TRACK_DEFINE(mycount, PERCETTO_TRACK_COUNTER);

static void test() {
  for (;;) {
    TRACE_EVENT(test, __func__);

    // This makes no sense, just demonstrates setting the same counter
    // from multiple threads.
    static int count = 1;
    count++;
    TRACE_COUNTER(test, mycount, count);
  }
}

int main(void) {
  int ret = PERCETTO_INIT(PERCETTO_CLOCK_DONT_CARE);
  if (ret != 0) {
    fprintf(stderr, "warning: failed to init tracing: %d\n", ret);
    // Note that tracing macros are safe regardless of percetto_init result.
  }

  ret = PERCETTO_REGISTER_TRACK(mycount);
  if (ret != 0) {
    fprintf(stderr, "warning: failed to register track: %d\n", ret);
    return -1;
  }

  constexpr int num_threads = 5;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(std::thread(test));
  }

  // runs continuously
  for (int i = 0; i < num_threads; ++i) {
    threads[i].join();
  }

  return 0;
}
