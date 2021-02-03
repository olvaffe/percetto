/*
 * Copyright (C) 2012 The Android Open Source Project
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

/**
 * Provides a compile-time compatibility layer from Android atrace macros to
 * the Percetto library. Better performance can be acheived by using the
 * Percetto macros and library directly.
 */

#ifndef PERCETTO_ATRACE_COMPAT_H
#define PERCETTO_ATRACE_COMPAT_H

#include <stdint.h>

#include "percetto.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copied from system/core/libcutils/include/cutils/trace.h
 */
#define ATRACE_TAG_NEVER            0       // This tag is never enabled.
#define ATRACE_TAG_ALWAYS           (1<<0)  // This tag is always enabled.
#define ATRACE_TAG_GRAPHICS         (1<<1)
#define ATRACE_TAG_INPUT            (1<<2)
#define ATRACE_TAG_VIEW             (1<<3)
#define ATRACE_TAG_WEBVIEW          (1<<4)
#define ATRACE_TAG_WINDOW_MANAGER   (1<<5)
#define ATRACE_TAG_ACTIVITY_MANAGER (1<<6)
#define ATRACE_TAG_SYNC_MANAGER     (1<<7)
#define ATRACE_TAG_AUDIO            (1<<8)
#define ATRACE_TAG_VIDEO            (1<<9)
#define ATRACE_TAG_CAMERA           (1<<10)
#define ATRACE_TAG_HAL              (1<<11)
#define ATRACE_TAG_APP              (1<<12)
#define ATRACE_TAG_RESOURCES        (1<<13)
#define ATRACE_TAG_DALVIK           (1<<14)
#define ATRACE_TAG_RS               (1<<15)
#define ATRACE_TAG_BIONIC           (1<<16)
#define ATRACE_TAG_POWER            (1<<17)
#define ATRACE_TAG_PACKAGE_MANAGER  (1<<18)
#define ATRACE_TAG_SYSTEM_SERVER    (1<<19)
#define ATRACE_TAG_DATABASE         (1<<20)
#define ATRACE_TAG_NETWORK          (1<<21)
#define ATRACE_TAG_ADB              (1<<22)
#define ATRACE_TAG_VIBRATOR         (1<<23)
#define ATRACE_TAG_AIDL             (1<<24)
#define ATRACE_TAG_NNAPI            (1<<25)
#define ATRACE_TAG_RRO              (1<<26)
#define ATRACE_TAG_SYSPROP          (1<<27)
#define ATRACE_TAG_LAST             ATRACE_TAG_SYSPROP

// Reserved for initialization.
#define ATRACE_TAG_NOT_READY        (1ULL<<63)

#define ATRACE_TAG_VALID_MASK ((ATRACE_TAG_LAST - 1) | ATRACE_TAG_LAST)

#ifndef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_NEVER
#elif ATRACE_TAG > ATRACE_TAG_VALID_MASK
#error ATRACE_TAG must be defined to be one of the tags defined in cutils/trace.h
#endif

__attribute__((visibility("default"))) void atrace_init();

__attribute__((visibility("default")))
void atrace_create_category(struct percetto_category** result, uint64_t tags);

__attribute__((visibility("default")))
void atrace_create_counter(struct percetto_track** result, const char* name);

__attribute__((visibility("default")))
void atrace_event(struct percetto_category* category,
                  uint32_t sessions,
                  int32_t type,
                  const struct percetto_event_data* data);

#ifdef NPERCETTO
#define ATRACE_INIT()
#define ATRACE_ANY(type, name, extra)
#define ATRACE_COUNTER(name, value)
#else

#define ATRACE_INIT() atrace_init()

#define ATRACE_ANY_WITH_ARGS_PTR(type, category, ptrack, ts, str_name, \
                                 extra_value) \
    do { \
      const uint32_t I_PERCETTO_UID(mask) = I_PERCETTO_LOAD_MASK_PTR(category); \
      if (PERCETTO_UNLIKELY(I_PERCETTO_UID(mask))) { \
        struct percetto_event_data I_PERCETTO_UID(data) = { \
          .track = ptrack, \
          .extra = (int64_t)(extra_value), \
          .timestamp = (ts), \
          .name = (str_name) \
        }; \
        atrace_event(category, I_PERCETTO_UID(mask), \
            (int32_t)(type), &I_PERCETTO_UID(data)); \
      } \
    } while(0)

#define ATRACE_ANY(type, name, extra) do { \
        static struct percetto_category* I_PERCETTO_UID(cat) = NULL; \
        if (PERCETTO_UNLIKELY(!I_PERCETTO_UID(cat))) \
            atrace_create_category(&I_PERCETTO_UID(cat), ATRACE_TAG); \
        ATRACE_ANY_WITH_ARGS_PTR(type, I_PERCETTO_UID(cat), NULL, 0, name, extra); \
    } while (0)

#define ATRACE_COUNTER(name, value) do { \
        static struct percetto_category* I_PERCETTO_UID(cat) = NULL; \
        static struct percetto_track* I_PERCETTO_UID(trk) = 0; \
        if (PERCETTO_UNLIKELY(!I_PERCETTO_UID(cat))) { \
            atrace_create_category(&I_PERCETTO_UID(cat), ATRACE_TAG); \
            atrace_create_counter(&I_PERCETTO_UID(trk), name); \
        } \
        ATRACE_ANY_WITH_ARGS_PTR(PERCETTO_EVENT_COUNTER, I_PERCETTO_UID(cat), \
            I_PERCETTO_UID(trk), 0, NULL, value); \
    } while (0)

#endif // NPERCETTO

#define ATRACE_BEGIN(name) ATRACE_ANY(PERCETTO_EVENT_BEGIN, name, 0)

#define ATRACE_END() ATRACE_ANY(PERCETTO_EVENT_END, NULL, 0)

#define ATRACE_ASYNC_BEGIN(name, cookie) \
    ATRACE_ANY(PERCETTO_EVENT_INSTANT, name, cookie)

#define ATRACE_ASYNC_END(name, cookie) \
    ATRACE_ANY(PERCETTO_EVENT_INSTANT, name, cookie)

#define ATRACE_INT(name, value) ATRACE_COUNTER(name, value)

#define ATRACE_INT64(name, value) ATRACE_COUNTER(name, value)

#ifdef __cplusplus
}
#endif

#endif // PERCETTO_ATRACE_COMPAT_H
