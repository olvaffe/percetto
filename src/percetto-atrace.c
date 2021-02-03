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

#include "percetto-atrace.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "percetto.h"


struct atrace_group_tracker {
  uint64_t tags[PERCETTO_MAX_GROUP_CATEGORIES];
  struct percetto_category categories[PERCETTO_MAX_GROUP_CATEGORIES];
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
PERCETTO_CATEGORY_DEFINE(never, "");

// The order in this array exactly matches the ATRACE_TAG bit shifts:
#define ATRACE_PERCETTO_CATEGORIES(C) \
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

PERCETTO_CATEGORY_DEFINE_MULTI(ATRACE_PERCETTO_CATEGORIES);

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

void atrace_create_category(struct percetto_category** result, uint64_t tags) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  if (s_percetto_status != PERCETTO_STATUS_OK ||
      !(tags & ATRACE_TAG_VALID_MASK)) {
    *result = PERCETTO_CATEGORY_PTR(never);
    return;
  }

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_groups.count; ++i) {
    if (s_groups.tags[i] == tags) {
      *result = &s_groups.categories[i];
      return;
    }
  }

  // Create the array of category indices for this group.
  const uint32_t static_count =
      sizeof(g_percetto_categories) / sizeof(g_percetto_categories[0]);
  assert(static_count <= 256);
  struct percetto_category_group group = { .child_ids = { 0 }, .count = 0 };
  for (uint32_t i = 0; i < static_count; ++i) {
    if (!!(tags & (1 << i))) {
      if (group.count == PERCETTO_MAX_GROUP_SIZE) {
        fprintf(stderr, "%s warning: too many categories in group\n",
                __func__);
        break;
      }
      group.child_ids[group.count++] = (uint8_t)i;
    }
  }

  if (group.count == 0) {
    // No categories were specified, so these trace events should never
    // trigger.
    *result = PERCETTO_CATEGORY_PTR(never);
    return;
  }

  if (group.count == 1) {
    // One category specified so return its pointer.
    uint32_t index = group.child_ids[0];
    assert(index < static_count);
    *result = g_percetto_categories[index];
    return;
  }

  if (s_groups.count == PERCETTO_MAX_GROUP_CATEGORIES) {
    fprintf(stderr, "%s error: too many different atrace combinations used\n",
            __func__);
    *result = PERCETTO_CATEGORY_PTR(never);
    return;
  }

  // Allocate and return a group category.
  struct percetto_category* category = &s_groups.categories[s_groups.count];
  s_groups.categories[s_groups.count] = PERCETTO_GROUP_CATEGORY(group);
  s_groups.tags[s_groups.count] = tags;
  ++s_groups.count;

  // Holding lock during register to avoid another thread using the same
  // category before it's ready.
  percetto_register_group_category(category);

  *result = category;
}

void atrace_create_counter(struct percetto_track** result, const char* name) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  if (s_percetto_status != PERCETTO_STATUS_OK)
    return;

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_tracks.count; ++i) {
    if (s_tracks.tracks[i].name == name) {
      *result = &s_tracks.tracks[i];
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

  *result = track;
}

void atrace_event(struct percetto_category* category,
                  uint32_t sessions,
                  int32_t type,
                  const struct percetto_event_data* data) {
  percetto_event(category, sessions, type, data);
}
