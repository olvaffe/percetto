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

#ifndef ATRACE_COMPAT_H
#define ATRACE_COMPAT_H

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

#define ATRACE_INIT() atrace_init()

void atrace_init();

struct percetto_category* atrace_create_category(uint64_t tags);

#define ATRACE_ANY(type, name, extra) do { \
        static struct percetto_category* PERCETTO_UID(cat) = NULL; \
        if (PERCETTO_UNLIKELY(!PERCETTO_UID(cat))) \
            PERCETTO_UID(cat) = atrace_create_category(ATRACE_TAG); \
        TRACE_ANY_WITH_ARGS_PTR(type, PERCETTO_UID(cat), 0, 0, name, extra); \
    } while (0)

#define ATRACE_BEGIN(name) ATRACE_ANY(PERCETTO_EVENT_BEGIN, name, 0)

#define ATRACE_END() ATRACE_ANY(PERCETTO_EVENT_END, NULL, 0)

#define ATRACE_ASYNC_BEGIN(name, cookie) \
    ATRACE_ANY(PERCETTO_EVENT_INSTANT, name, cookie)

#define ATRACE_ASYNC_END(name, cookie) \
    ATRACE_ANY(PERCETTO_EVENT_INSTANT, name, cookie)

#define ATRACE_INT(name, value) // TODO

#define ATRACE_INT64(name, value) // TODO

#ifdef __cplusplus
}
#endif

#endif // ATRACE_COMPAT_H
