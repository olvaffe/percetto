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

static void test() {
  for (;;) {
    TRACE_EVENT(test, __func__);
  }
}

int main(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(test),
  };
  int ret = percetto_init(sizeof(categories) / sizeof(categories[0]),
                          categories, PERCETTO_CLOCK_DONT_CARE);
  if (ret != 0) {
    fprintf(stderr, "warning: failed to init tracing: %d\n", ret);
    // Note that tracing macros are safe regardless of percetto_init result.
  }

  constexpr int num_threads = 5;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(std::thread(test));
  }

  // runs continuously

  return 0;
}
