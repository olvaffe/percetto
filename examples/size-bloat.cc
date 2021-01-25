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

#include "perfetto.h"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("test").SetDescription("test category"));

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

static void test(void) {
  TRACE_EVENT("test", __func__);

  {
    TRACE_EVENT("test", "nested scope");

    TRACE_EVENT_BEGIN("test", "manual");
    TRACE_EVENT_END("test");
  }
}

int main(void) {
  const int wait = 60;
  const int event_count = 100;
  int i;

  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  perfetto::TrackEvent::Register();

  for (i = 0; i < wait; i++) {
    if (TRACE_EVENT_CATEGORY_ENABLED("test"))
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
