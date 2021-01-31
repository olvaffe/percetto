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
#include <time.h>

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

/** Optionally declare each category in a header file. */
#define PERCETTO_CATEGORY_DECLARE(category) \
    extern struct percetto_category g_percetto_category_##category

/** Define each category with tags in a compilation file. */
#define PERCETTO_CATEGORY_DEFINE_WITH_TAGS(category, description, ...) \
    struct percetto_category g_percetto_category_##category = \
        { PERCETTO_ATOMIC_INIT(0), #category, description, {__VA_ARGS__}, \
        { { 0 }, 0 }, { 0 } }

/** Define a category in a compilation file. */
#define PERCETTO_CATEGORY_DEFINE(category, description) \
    PERCETTO_CATEGORY_DEFINE_WITH_TAGS(category, description, NULL)

/** Define a slow category (disabled by default) in a compilation file. */
#define PERCETTO_CATEGORY_DEFINE_SLOW(category, description) \
    PERCETTO_CATEGORY_DEFINE_WITH_TAGS(category, description, "slow")

/** /group/ is a percetto_category_group. */
#define PERCETTO_GROUP_CATEGORY(group) (struct percetto_category) \
    { PERCETTO_ATOMIC_INIT(0), NULL, NULL, { NULL }, group, { 0 } }

/** Declare each track in a header file. */
#define PERCETTO_TRACK_DECLARE(track) \
    extern struct percetto_track g_percetto_track_##track

/**
 * Define each track in a compilation file. For /track_type/ see
 * percetto_track_type.
 */
#define PERCETTO_TRACK_DEFINE(track, track_type, track_uuid_ui64) \
    struct percetto_track g_percetto_track_##track = \
        { track_uuid_ui64, #track, (int32_t)track_type, { 0 } }

#define PERCETTO_TRACK_PTR(track) (&g_percetto_track_##track)

#define PERCETTO_CATEGORY_PTR(category) (&g_percetto_category_##category)

#define PERCETTO_CATEGORY_IS_ENABLED(category) \
    (!!g_percetto_category_##category.sessions)

#define PERCETTO_MAX_CATEGORIES 256
#define PERCETTO_MAX_GROUP_CATEGORIES 32
#define PERCETTO_MAX_GROUP_SIZE 4
#define PERCETTO_MAX_CATEGORY_TAGS 4
#define PERCETTO_MAX_TRACKS 32

enum percetto_clock {
  /* Same as perfetto BuiltinClock. */
  PERCETTO_CLOCK_DONT_CARE = 0,
  PERCETTO_CLOCK_REALTIME = 1,
  PERCETTO_CLOCK_REALTIME_COARSE = 2,
  PERCETTO_CLOCK_MONOTONIC = 3,
  PERCETTO_CLOCK_MONOTONIC_COARSE = 4,
  PERCETTO_CLOCK_MONOTONIC_RAW = 5,
  PERCETTO_CLOCK_BOOTTIME = 6,
};

enum percetto_event_type {
  /* Same as perfetto TrackEvent_Type. */
  PERCETTO_EVENT_UNSPECIFIED = 0,
  PERCETTO_EVENT_BEGIN = 1,
  PERCETTO_EVENT_END = 2,
  PERCETTO_EVENT_INSTANT = 3,
  PERCETTO_EVENT_COUNTER = 4,
};

enum percetto_event_extended_type {
  PERCETTO_EVENT_EXTENDED_DEBUG_DATA = 0,
  PERCETTO_EVENT_EXTENDED_END,
};

enum percetto_event_debug_data_type {
  PERCETTO_EVENT_DEBUG_DATA_BOOL = 0,
  PERCETTO_EVENT_DEBUG_DATA_UINT,
  PERCETTO_EVENT_DEBUG_DATA_INT,
  PERCETTO_EVENT_DEBUG_DATA_DOUBLE,
  PERCETTO_EVENT_DEBUG_DATA_STRING,
  PERCETTO_EVENT_DEBUG_DATA_POINTER,
  PERCETTO_EVENT_DEBUG_DATA_END,
};

enum percetto_track_type {
  PERCETTO_TRACK_NORMAL = 0,
  PERCETTO_TRACK_COUNTER,
};

struct percetto_category_group {
  /* For group categories, two or more of these child_ids are set to the
   * indices of corresponding categories. */
  uint8_t child_ids[PERCETTO_MAX_GROUP_SIZE];
  uint32_t count;
};

struct percetto_category {
  atomic_uint_fast32_t sessions;
  const char* name;
  const char* description;
  const char* tags[PERCETTO_MAX_CATEGORY_TAGS];
  /* Only used for group categories. */
  struct percetto_category_group group;
  uint64_t _reserved[3];
};

struct percetto_track {
  uint64_t uuid;
  const char* name;
  /* See percetto_track_type */
  int32_t type;
  uint8_t _reserved[64];
};

struct percetto_event_data {
  /* Non-zero implies target track, otherwise thread track is used. */
  uint64_t track_uuid;
  int64_t extra;
  /* Non-zero implies provided timestamp. */
  uint64_t timestamp;
  const char* name;
};

struct percetto_event_extended {
  /* See percetto_event_extended_type. */
  int32_t type;
  const struct percetto_event_extended* next;
};

struct percetto_event_debug_data {
  struct percetto_event_extended extended;
  /* See percetto_event_debug_data_type. */
  int32_t type;
  const char* name;
  union {
    uint32_t bool_value;
    uint64_t uint_value;
    int64_t int_value;
    double double_value;
    const char* string_value;
    uintptr_t pointer_value;
  };
};

/**
 * Initialize the Percetto library.
 * Not thread safe. Only one call is allowed.
 *
 * /categories/ param must be a pointer to static storage.
 * /clock_id/ is the clock that will be used for all trace event timestamps,
 *   whether passed through the API or retrieved internally.
 *   BUILTIN_CLOCK_BOOTTIME is the recommended choice, but some systems may not
 *   support it. PERCETTO_CLOCK_DONT_CARE will try to use BOOTTIME and fall
 *   back onto MONOTONIC. If you plan to manually set timestamps on any events,
 *   you must confirm that the clock works (ie: by checking the result of
 *   clock_gettime) and then pass in the corresponding PERCETTO_CLOCK enum
 *   rather than using PERCETTO_CLOCK_DONT_CARE.
 *
 * Returns 0 on success or negative error code on failure. After failure, it is
 * safe to continue the application and the calls to trace macros below will
 * behave as if tracing is always disabled.
 * TODO(jbates): document all error codes.
 */
int percetto_init(size_t category_count,
                  struct percetto_category** categories,
                  enum percetto_clock clock_id);

/**
 * Up to PERCETTO_MAX_TRACKS tracks can be added for counters or flow events.
 * Tracks can never be removed for the lifetime of the process.
 * Thread safe. Can be called from any thread after percetto_init.
 */
int percetto_register_track(struct percetto_track* track);

/**
 * Up to PERCETTO_MAX_GROUP_CATEGORIES can be added.
 * Categories can never be removed for the lifetime of the process.
 * Thread safe. Can be called from any thread after percetto_init.
 */
int percetto_register_group_category(struct percetto_category* category);

/** See TRACE_EVENT macros. */
void percetto_event_begin(struct percetto_category* category,
                          uint32_t sessions,
                          const char* name);

/** See TRACE_EVENT macros. */
void percetto_event_end(struct percetto_category* category,
                        uint32_t sessions);

/** See TRACE_INSTANT, TRACE_COUNTER, TRACE_ANY_WITH_ARGS macros. */
void percetto_event(struct percetto_category* category,
                    uint32_t sessions,
                    int32_t type,
                    const struct percetto_event_data* data);

/** See TRACE_DEBUG macros. */
void percetto_event_extended(struct percetto_category* category,
                             uint32_t sessions,
                             int32_t type,
                             const struct percetto_event_data* data,
                             const struct percetto_event_extended* extended);

static inline void percetto_cleanup_end(struct percetto_category** category) {
  const uint32_t mask = atomic_load_explicit(&(*category)->sessions,
                                             memory_order_relaxed);
  if (PERCETTO_UNLIKELY(mask))
    percetto_event_end(*category, mask);
}

#define PERCETTO_LOAD_MASK_PTR(category) \
    atomic_load_explicit(&(category)->sessions, memory_order_relaxed)

#define PERCETTO_LOAD_MASK(category) \
    PERCETTO_LOAD_MASK_PTR(&g_percetto_category_##category)

/**
 * Trace the current scope. /str_name/ is only evaluated when tracing is
 * enabled.
 */
#define TRACE_EVENT(category, str_name) \
    const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
    if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) \
      percetto_event_begin(&g_percetto_category_##category, \
          PERCETTO_UID(mask), (str_name)); \
    struct percetto_category* PERCETTO_UID(trace_scoped) \
        __attribute__((cleanup(percetto_cleanup_end), unused)) = \
            &g_percetto_category_##category

#define TRACE_ANY_WITH_ARGS_PTR(type, category, track_id, ts, str_name, \
                                extra_value) \
    do { \
      const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK_PTR(category); \
      if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) { \
        struct percetto_event_data PERCETTO_UID(data) = { \
          .track_uuid = (uint64_t)(track_id), \
          .extra = (int64_t)(extra_value), \
          .timestamp = (ts), \
          .name = (str_name) \
        }; \
        percetto_event(category, PERCETTO_UID(mask), \
            (int32_t)(type), &PERCETTO_UID(data)); \
      } \
    } while(0)

#define TRACE_ANY_WITH_ARGS(type, category, track_id, ts, str_name, \
                            extra_value) \
    TRACE_ANY_WITH_ARGS_PTR(type, &g_percetto_category_##category, track_id, \
        ts, str_name, extra_value)

#define TRACE_EVENT_BEGIN(category, str_name) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_BEGIN, category, 0, 0, str_name, 0)

#define TRACE_EVENT_END(category) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_END, category, 0, 0, NULL, 0)

#define TRACE_EVENT_BEGIN_ON_TRACK(category, track, timestamp, str_name) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_BEGIN, category, \
        g_percetto_track_##track.uuid, timestamp, str_name, 0)

#define TRACE_EVENT_END_ON_TRACK(category, track, timestamp) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_END, category, \
        g_percetto_track_##track.uuid, timestamp, NULL, 0)

#define TRACE_INSTANT(category, str_name) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_INSTANT, category, 0, 0, str_name, 0)

#define TRACE_INSTANT_ON_TRACK(category, track, str_name) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_INSTANT, category, \
        g_percetto_track_##track.uuid, 0, str_name, 0)

#define TRACE_COUNTER(category, track, i64_value) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_COUNTER, category, \
        g_percetto_track_##track.uuid, 0, NULL, i64_value)

#define TRACE_FLOW(category, str_name, i64_cookie) \
    TRACE_ANY_WITH_ARGS(PERCETTO_EVENT_INSTANT, category, \
        0, 0, str_name, i64_cookie)

/* Debug annotation macros */

#define PERCETTO_BOOL(str_name, value) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_BOOL, \
      .name = str_name, .bool_value = (uint32_t)!!(value) \
    }

#define PERCETTO_UINT(str_name, value) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_UINT, \
      .name = str_name, .uint_value = (value) \
    }

#define PERCETTO_INT(str_name, value) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_INT, \
      .name = str_name, .int_value = (value) \
    }

#define PERCETTO_DOUBLE(str_name, value) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_DOUBLE, \
      .name = str_name, .double_value = (value) \
    }

#define PERCETTO_STRING(str_name, str_value) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_STRING, \
      .name = str_name, .string_value = (str_value) \
    }

#define PERCETTO_POINTER(str_name, ptr) { \
      .extended = { .type = PERCETTO_EVENT_EXTENDED_DEBUG_DATA, .next = NULL }, \
      .type = PERCETTO_EVENT_DEBUG_DATA_POINTER, \
      .name = str_name, .pointer_value = (uintptr_t)(ptr) \
    }

#define TRACE_DEBUG_DATA(category, data) \
    do { \
      const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
      if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) { \
        struct percetto_event_debug_data PERCETTO_UID(dbg1) = data; \
        struct percetto_event_data PERCETTO_UID(evdata) = { \
          .track_uuid = 0, \
          .extra = 0, \
          .timestamp = 0, \
          .name = PERCETTO_UID(dbg1).name \
        }; \
        percetto_event_extended(&g_percetto_category_##category, \
            PERCETTO_UID(mask), (int32_t)PERCETTO_EVENT_INSTANT, \
            &PERCETTO_UID(evdata), &PERCETTO_UID(dbg1).extended); \
      } \
    } while(0)

#define TRACE_DEBUG_DATA2(category, str_name, data1, data2) \
    do { \
      const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
      if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) { \
        struct percetto_event_data PERCETTO_UID(evdata) = { \
          .track_uuid = 0, \
          .extra = 0, \
          .timestamp = 0, \
          .name = (str_name) \
        }; \
        struct percetto_event_debug_data PERCETTO_UID(dbg1) = data1; \
        struct percetto_event_debug_data PERCETTO_UID(dbg2) = data2; \
        PERCETTO_UID(dbg1).extended.next = &PERCETTO_UID(dbg2).extended; \
        percetto_event_extended(&g_percetto_category_##category, \
            PERCETTO_UID(mask), (int32_t)PERCETTO_EVENT_INSTANT, \
            &PERCETTO_UID(evdata), &PERCETTO_UID(dbg1).extended); \
      } \
    } while(0)

#define TRACE_DEBUG_DATA3(category, str_name, data1, data2, data3) \
    do { \
      const uint32_t PERCETTO_UID(mask) = PERCETTO_LOAD_MASK(category); \
      if (PERCETTO_UNLIKELY(PERCETTO_UID(mask))) { \
        struct percetto_event_data PERCETTO_UID(evdata) = { \
          .track_uuid = 0, \
          .extra = 0, \
          .timestamp = 0, \
          .name = (str_name) \
        }; \
        struct percetto_event_debug_data PERCETTO_UID(dbg1) = data1; \
        struct percetto_event_debug_data PERCETTO_UID(dbg2) = data2; \
        struct percetto_event_debug_data PERCETTO_UID(dbg3) = data3; \
        PERCETTO_UID(dbg1).extended.next = &PERCETTO_UID(dbg2).extended; \
        PERCETTO_UID(dbg2).extended.next = &PERCETTO_UID(dbg3).extended; \
        percetto_event_extended(&g_percetto_category_##category, \
            PERCETTO_UID(mask), (int32_t)PERCETTO_EVENT_INSTANT, \
            &PERCETTO_UID(evdata), &PERCETTO_UID(dbg1).extended); \
      } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PERCETTO_H */
