/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
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
  const int wait = 5;
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
  test();

  return 0;
}
