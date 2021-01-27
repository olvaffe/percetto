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

PERCETTO_CATEGORY_DEFINE(test, "Test events", 0);

static int trace_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories);
}

static void test(int iterations) {
  for (int i = 0; i < iterations; ++i) {
    TRACE_EVENT(test, __func__);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
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

int main(void) {
  int ret = trace_init();
  if (ret != 0) {
    fprintf(stderr, "failed to init tracing: %d\n", ret);
    return -1;
  }

  wait_for_tracing(true);

  constexpr int num_threads = 5;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(std::thread(test, 100));
  }

  for (int i = 0; i < num_threads; ++i) {
    threads[i].join();
  }

  return 0;
}
