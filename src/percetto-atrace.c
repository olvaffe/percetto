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

// NPERCETTO does not apply to the percetto-atrace library.
#ifdef NPERCETTO
#undef NPERCETTO
#endif

#include "percetto-atrace.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "percetto.h"

struct atrace_group_tracker {
  uint64_t tags[PERCETTO_MAX_GROUP_CATEGORIES];
  struct percetto_category categories[PERCETTO_MAX_GROUP_CATEGORIES];
  struct percetto_category_ext category_exts[PERCETTO_MAX_GROUP_CATEGORIES];
  int count;
};

struct atrace_counter_tracker {
  struct percetto_track tracks[PERCETTO_MAX_TRACKS];
  int count;
};

enum percetto_status {
  PERCETTO_STATUS_NOT_STARTED = 0,
  PERCETTO_STATUS_OK,
  PERCETTO_STATUS_ERROR,
};

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static volatile enum percetto_status s_percetto_status =
    PERCETTO_STATUS_NOT_STARTED;

static struct atrace_group_tracker s_groups;

static struct atrace_counter_tracker s_tracks;

// This category is never registered so it remains disabled permanently.
struct percetto_category s_category_never = { ATOMIC_VAR_INIT(0), NULL, NULL };

// The order in this array exactly matches the ATRACE_TAG bit shifts:
#define ATRACE_PERCETTO_CATEGORIES(C, G) \
  C(always, "ATRACE_TAG_ALWAYS") \
  C(gfx, "Graphics", "slow") \
  C(input, "Input", "slow") \
  C(view, "View System", "slow") \
  C(webview, "WebView", "slow") \
  C(wm, "Window Manager", "slow") \
  C(am, "Activity Manager", "slow") \
  C(sm, "Sync Manager", "slow") \
  C(audio, "Audio", "slow") \
  C(video, "Video", "slow") \
  C(camera, "Camera", "slow") \
  C(hal, "Hardware Modules", "slow") \
  C(app, "ATRACE_TAG_APP", "slow") \
  C(res, "ATRACE_TAG_VIEW", "slow") \
  C(dalvik, "Dalvik VM", "slow") \
  C(rs, "RenderScript", "slow") \
  C(bionic, "Bionic C Library", "slow") \
  C(power, "Power Management", "slow") \
  C(pm, "Package Manager", "slow") \
  C(ss, "System Server", "slow") \
  C(database, "Database", "slow") \
  C(network, "Network", "slow") \
  C(adb, "ADB", "slow") \
  C(vibrator, "Vibrator", "slow") \
  C(aidl, "AIDL calls", "slow") \
  C(nnapi, "NNAPI", "slow") \
  C(rro, "ATRACE_TAG_RRO", "slow") \
  C(sysprop, "ATRACE_TAG_SYSPROP", "slow")

PERCETTO_CATEGORY_DEFINE(ATRACE_PERCETTO_CATEGORIES);

#define SCOPED_LOCK(mutex) \
   pthread_mutex_lock(&(mutex)); \
   pthread_mutex_t* I_PERCETTO_UID(unlocker) \
      __attribute__((cleanup(scoped_unlock), unused)) = &(mutex)

static inline void scoped_unlock(pthread_mutex_t** mutex) {
  pthread_mutex_unlock(*mutex);
}

void atrace_init() {
  SCOPED_LOCK(s_lock);
  if (s_percetto_status != PERCETTO_STATUS_NOT_STARTED)
    return;

  int ret = PERCETTO_INIT(PERCETTO_CLOCK_DONT_CARE);
  if (ret < 0) {
    fprintf(stderr, "error: percetto_init failed: %d\n", ret);
    s_percetto_status = PERCETTO_STATUS_ERROR;
  } else {
    s_percetto_status = PERCETTO_STATUS_OK;
  }
}

static void atrace_create_category_locked(atomic_uintptr_t* result,
                                          uint64_t tags) {
  if (s_percetto_status != PERCETTO_STATUS_OK ||
      !(tags & ATRACE_TAG_VALID_MASK)) {
    *result = (uintptr_t)&s_category_never;
    return;
  }

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_groups.count; ++i) {
    if (s_groups.tags[i] == tags) {
      *result = (uintptr_t)&s_groups.categories[i];
      return;
    }
  }

  // Create the array of category indices for this group.
  const uint32_t static_count =
      sizeof(g_percetto_categories) / sizeof(g_percetto_categories[0]);
  assert(static_count <= 256);
  uint32_t group_size = 0;
  struct percetto_category* group[PERCETTO_MAX_GROUP_SIZE];
  for (uint32_t i = 0; i < static_count; ++i) {
    if (!!(tags & (1 << i))) {
      if (group_size == PERCETTO_MAX_GROUP_SIZE) {
        fprintf(stderr, "%s warning: too many categories in group\n",
                __func__);
        break;
      }
      group[group_size++] = g_percetto_categories[i];
    }
  }

  if (group_size == 0) {
    // No categories were specified, so these trace events should never
    // trigger.
    *result = (uintptr_t)&s_category_never;
    return;
  }

  if (group_size == 1) {
    // One category specified so return its pointer.
    *result = (uintptr_t)group[0];
    return;
  }

  if (s_groups.count == PERCETTO_MAX_GROUP_CATEGORIES) {
    fprintf(stderr, "%s error: too many different atrace combinations used\n",
            __func__);
    *result = (uintptr_t)&s_category_never;
    return;
  }

  // Allocate and return a group category.
  struct percetto_category* category = &s_groups.categories[s_groups.count];
  for (uint32_t i = 0; i < group_size; ++i)
    s_groups.category_exts[s_groups.count].group[i] = group[i];
  s_groups.categories[s_groups.count] = (struct percetto_category)
      { 0, NULL, &s_groups.category_exts[s_groups.count] };
  s_groups.tags[s_groups.count] = tags;
  ++s_groups.count;

  // Holding lock during register to avoid another thread using the same
  // category before it's ready.
  percetto_register_group_category(category);

  atomic_store_explicit(result, (uintptr_t)category, memory_order_release);
}

static void atrace_create_counter_locked(atomic_uintptr_t* result,
                                         const char* name) {
  if (s_percetto_status != PERCETTO_STATUS_OK)
    return;

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_tracks.count; ++i) {
    if (s_tracks.tracks[i].name == name) {
      *result = (uintptr_t)&s_tracks.tracks[i];
      return;
    }
  }

  if (s_tracks.count == PERCETTO_MAX_TRACKS) {
    fprintf(stderr, "%s error: too many atrace counters used\n", __func__);
    return;
  }

  struct percetto_track* track = &s_tracks.tracks[s_tracks.count];
  ++s_tracks.count;
  track->name = name;
  track->type = PERCETTO_TRACK_COUNTER;

  // Holding lock during register to avoid another thread using the same track
  // before it's ready.
  percetto_register_track(track);

  atomic_store_explicit(result, (uintptr_t)track, memory_order_release);
}

void atrace_create_category(atomic_uintptr_t* result, uint64_t tags) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  atrace_create_category_locked(result, tags);
}

void atrace_create_category_and_counter(
    atomic_uintptr_t* out_category,
    uint64_t category_tags,
    atomic_uintptr_t* out_track,
    const char* track_name) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  // The macros load category pointer and then counter pointer,
  // so make sure counter is stored before category.
  atrace_create_counter_locked(out_track, track_name);
  atrace_create_category_locked(out_category, category_tags);
}

void atrace_event(struct percetto_category* category,
                  uint32_t sessions,
                  int32_t type,
                  const struct percetto_event_data* data) {
  percetto_event(category, sessions, type, data);
}
