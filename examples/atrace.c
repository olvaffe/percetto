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

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "percetto-atrace.h"

extern void atrace2(void);

int main(void) {
  for (;;) {
    for (int i = 0; i < 10; i++) {
      ATRACE_BEGIN(__func__);
      ATRACE_INT("i", i);
      atrace2();
      ATRACE_END();
    }
    struct timespec t = {0, 10000000};
    nanosleep(&t, NULL);
  }

  return 0;
}
