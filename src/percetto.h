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

#ifndef PERCETTO_H
#define PERCETTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_fast32_t;
using std::atomic_load_explicit;
using std::memory_order_relaxed;
using std::size_t;
#define PERCETTO_ATOMIC_INIT(n) {n}
#else
#include <stdatomic.h>
#define PERCETTO_ATOMIC_INIT(n) n
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PERCETTO_LIKELY(x) __builtin_expect(!!(x), 1)
#define PERCETTO_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define PERCETTO_UID3(a, b) percetto_uid_##a##b
#define PERCETTO_UID2(a, b) PERCETTO_UID3(a, b)
#define PERCETTO_UID(prefix) PERCETTO_UID2(prefix, __LINE__)

/* This category must be explicitly requested, disabled by default. */
#define PERCETTO_CATEGORY_FLAG_SLOW   (1 << 0)
/* More verbose debug data, disabled by default. */
#define PERCETTO_CATEGORY_FLAG_DEBUG  (1 << 1)

/* Declare each category in a header file. */
#define PERCETTO_CATEGORY_DECLARE(category) \
    extern struct percetto_category g_percetto_category_##category
/* Define each category in a compilation file with optional flags. */
#define PERCETTO_CATEGORY_DEFINE(category, description, flags) \
    struct percetto_category g_percetto_category_##category = \
        { PERCETTO_ATOMIC_INIT(0), #category, description, flags, { 0 } }

/* Declare each track in a header file. */
#define PERCETTO_TRACK_DECLARE(track) \
    extern struct percetto_track g_percetto_track_##track
/* Define each track in a compilation file. */
#define PERCETTO_TRACK_DEFINE(track, track_uuid_ui64) \
    struct percetto_track g_percetto_track_##track = \
        { track_uuid_ui64, #track, { 0 } }
#define PERCETTO_TRACK_PTR(track) (&g_percetto_track_##track)

#define PERCETTO_CATEGORY_PTR(category) (&g_percetto_category_##category)

#define PERCETTO_CATEGORY_IS_ENABLED(category) \
    (!!g_percetto_category_##category.sessions)

#define PERCETTO_MAX_TRACKS 32

enum percetto_event_type {
  // Same as perfetto TrackEvent_Type.
  PERCETTO_EVENT_UNSPECIFIED = 0,
  PERCETTO_EVENT_BEGIN = 1,
  PERCETTO_EVENT_END = 2,
  PERCETTO_EVENT_INSTANT = 3,
  PERCETTO_EVENT_COUNTER = 4,
};

// TODO: add support for trace events with custom timestamps.
// TODO: add support for extra parameters on trace events.

struct percetto_category {
  atomic_uint_fast32_t sessions;
  const char* name;
  const char* description;
  /* See PERCETTO_CATEGORY_FLAG_* */
  const uint64_t flags;
  uint8_t _reserved[32];
};

struct percetto_track {
  const uint64_t uuid;
  const char* name;
  uint8_t _reserved[64];
};

/* categories param must be a pointer to static storage. */
int percetto_init(size_t category_count,
                  struct percetto_category** categories);

/* up to PERCETTO_MAX_TRACKS tracks can be added for counters or flow events */
int percetto_register_track(struct percetto_track* track);

void percetto_event_begin(struct percetto_category* category,
                          uint32_t sessions,
                          const char* name);

void percetto_event_end(struct percetto_category* category,
                        uint32_t sessions);

void percetto_event(struct percetto_category* category,
                    uint32_t sessions,
                    const char* name,
                    enum percetto_event_type type,
                    uint64_t track_uuid,
                    int64_t extra);

static inline void percetto_cleanup_end(struct percetto_category** category) {
  const uint32_t mask = atomic_load_explicit(&(*category)->sessions,
                                             memory_order_relaxed);
  if (PERCETTO_UNLIKELY(mask))
    percetto_event_end(*category, mask);
}

#define PERCETTO_LOAD_MASK(category) \
    atomic_load_explicit(&g_percetto_category_##category.sessions, \
        memory_order_relaxed)

/* Trace the current scope. /name/ is only evaluated when tracing is
 * enabled. */
#define TRACE_EVENT(category, name) \
    const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
    if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) \
      percetto_event_begin(&g_percetto_category_##category, \
          PERCETTO_UID(mask), (name)); \
    struct percetto_category* PERCETTO_UID(trace_scoped) \
        __attribute__((cleanup(percetto_cleanup_end), unused)) = \
            &g_percetto_category_##category

#define TRACE_INSTANT(category, name) \
    const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
    if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) \
      percetto_event(&g_percetto_category_##category, PERCETTO_UID(mask), \
          (name), PERCETTO_EVENT_INSTANT, 0, 0)

#define TRACE_COUNTER(category, track, i64_value) \
    const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
    if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) \
      percetto_event(&g_percetto_category_##category, PERCETTO_UID(mask), \
          NULL, PERCETTO_EVENT_COUNTER, g_percetto_track_##track.uuid, \
          (int64_t)(i64_value))

#define TRACE_FLOW(category, name, cookie) \
    const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
    if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) \
      percetto_event(&g_percetto_category_##category, PERCETTO_UID(mask), \
          (name), PERCETTO_EVENT_INSTANT, 0, (int64_t)(cookie))

#ifdef __cplusplus
}
#endif

#endif /* PERCETTO_H */
