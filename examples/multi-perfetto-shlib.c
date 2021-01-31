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

#include "percetto.h"
#include "multi-perfetto-shlib.h"

PERCETTO_CATEGORY_DEFINE(shlib, "Shared lib test events");

int test_shlib_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(shlib),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories,
                       PERCETTO_CLOCK_DONT_CARE);
}

void test_shlib_func(int64_t flow_id) {
  TRACE_EVENT(shlib, "test_shlib_func");
  TRACE_FLOW(shlib, "flow2", flow_id);
}