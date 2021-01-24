/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef PERCETTO_H
#define PERCETTO_H

#include <stdbool.h>
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

#define PERCETTO_LIKELY(x) __builtin_expect(!!(x), 1)
#define PERCETTO_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define PERCETTO_UID3(a, b) percetto_uid_##a##b
#define PERCETTO_UID2(a, b) PERCETTO_UID3(a, b)
#define PERCETTO_UID(prefix) PERCETTO_UID2(prefix, __LINE__)

#define PERCETTO_CATEGORY_PTR(category) (&g_percetto_category_##category)

#define PERCETTO_CATEGORY_IS_ENABLED(category) \
    (!!g_percetto_category_##category.sessions)

#define TRACE_EVENT(category, name) \
    struct percetto_category* PERCETTO_UID(trace_scoped) \
        __attribute__((cleanup(percetto_trace_end), unused)) = \
            percetto_trace_begin(&g_percetto_category_##category, name)

// TODO: add support for trace events with custom timestamps.
// TODO: add support for extra parameters on trace events.
// TODO: add support for flow events.

struct percetto_category {
  atomic_uint_fast32_t sessions;
  const char* name;
  const char* description;
  /* See PERCETTO_CATEGORY_FLAG_* */
  const uint64_t flags;
  uint8_t _reserved[32];
};

/* categories param must be a pointer to static storage. */
bool percetto_init(size_t category_count,
                   struct percetto_category** categories);

void percetto_slice_begin(struct percetto_category* category,
                          uint32_t instance_mask,
                          const char* name);

void percetto_slice_end(struct percetto_category* category,
                        uint32_t instance_mask);

void percetto_instant(struct percetto_category* category,
                      uint32_t instance_mask,
                      const char* name);

static inline struct percetto_category* percetto_trace_begin(
    struct percetto_category* category, const char* name) {
  const uint32_t mask = atomic_load_explicit(&category->sessions,
                                             memory_order_relaxed);
  if (PERCETTO_UNLIKELY(mask))
    percetto_slice_begin(category, mask, name);
  return category;
}

static inline void percetto_trace_end(struct percetto_category** category) {
  const uint32_t mask = atomic_load_explicit(&(*category)->sessions,
                                             memory_order_relaxed);
  if (PERCETTO_UNLIKELY(mask))
    percetto_slice_end(*category, mask);
}

#ifdef __cplusplus
}
#endif

#endif /* PERCETTO_H */
