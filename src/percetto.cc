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

#include "percetto.h"

#include <array>
#include <atomic>
#include <perfetto.h>

#include "perfetto-port.h"

namespace {

using perfetto::protos::gen::TrackDescriptor;
using perfetto::protos::gen::TrackEventConfig;
using perfetto::protos::pbzero::BuiltinClock;
using perfetto::protos::pbzero::CounterDescriptor_Unit_UNIT_COUNT;
using perfetto::protos::pbzero::DataSourceDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDescriptor;

struct Percetto {
  int is_initialized;
  struct percetto_category** categories;
  int category_count;
  std::array<std::atomic<struct percetto_category*>,
                         PERCETTO_MAX_GROUP_CATEGORIES> groups;
  std::array<std::atomic<struct percetto_track*>, PERCETTO_MAX_TRACKS> tracks;
  clockid_t trace_clock_id;
  BuiltinClock perfetto_clock;
};

static Percetto s_percetto;

// Tracks the last incremental update performed by each thread.
// When this is less than s_target_incremental_update, it means
// that the thread needs to clear and update the perfetto incremental state.
static thread_local int s_committed_incremental_update = 0;
static std::atomic_int s_target_incremental_update(0);

static bool CheckSystemClock(clockid_t system_clock) {
  struct timespec ts = {};
  return (clock_gettime(system_clock, &ts) == 0);
}

static clockid_t DetermineClockId(BuiltinClock* result) {
  // Determine clock to use (follows perfetto's preference for BOOTTIME).
  if (CheckSystemClock(CLOCK_BOOTTIME)) {
    *result = perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME;
    return CLOCK_BOOTTIME;
  }
  if (CheckSystemClock(CLOCK_MONOTONIC)) {
    *result = perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC;
    return CLOCK_MONOTONIC;
  }
  *result = perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME;
  return CLOCK_REALTIME;
}

static clockid_t GetClockIdFrom(BuiltinClock perfetto_clock,
                                BuiltinClock* result) {
  switch(perfetto_clock) {
    default:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME;
      return CLOCK_REALTIME;
    case perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME_COARSE:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME_COARSE;
      return CLOCK_REALTIME_COARSE;
    case perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC;
      return CLOCK_MONOTONIC;
    case perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_COARSE:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_COARSE;
      return CLOCK_MONOTONIC_COARSE;
    case perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_RAW:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_RAW;
      return CLOCK_MONOTONIC_RAW;
    case perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME:
      *result = perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME;
      return CLOCK_BOOTTIME;
  }
}

static inline uint64_t GetTimestampNs() {
  struct timespec ts = {};
  clock_gettime(s_percetto.trace_clock_id, &ts);
  return static_cast<uint64_t>(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

class PercettoDataSource : public perfetto::DataSource<PercettoDataSource> {
  using Base = DataSource<PercettoDataSource>;

 public:
  // Cause all threads to clear and update incremental perfetto data the next
  // time they send a trace event.
  // Thread safe.
  static void KickIncrementalUpdates() {
    s_target_incremental_update.fetch_add(1, std::memory_order_acq_rel);
  }

  void OnSetup(const DataSourceBase::SetupArgs& args) override {
    PERFETTO_DCHECK(args.config);
    if (!args.config)
      return;
    TrackEventConfig config;
    const auto& config_raw = args.config->track_event_config_raw();
    bool ok = config.ParseFromArray(config_raw.data(), config_raw.size());
    PERFETTO_DCHECK(ok);
    if (!ok)
      return;
    for (int i = 0; i < s_percetto.category_count; i++) {
      std::array<const char*, PERCETTO_MAX_CATEGORY_TAGS> tags;
      std::copy(std::begin(s_percetto.categories[i]->tags),
                std::end(s_percetto.categories[i]->tags), std::begin(tags));
      if (IsCategoryEnabled(s_percetto.categories[i]->name, tags, config)) {
        std::atomic_fetch_or(&s_percetto.categories[i]->sessions,
                             1 << args.internal_instance_index);
      }
    }
    UpdateGroupCategories();
    KickIncrementalUpdates();
  }

  void OnStart(const DataSourceBase::StartArgs&) override {}

  void OnStop(const DataSourceBase::StopArgs& args) override {
    for (int i = 0; i < s_percetto.category_count; i++) {
      std::atomic_fetch_and(&s_percetto.categories[i]->sessions,
          ~(1 << args.internal_instance_index));
    }
    UpdateGroupCategories();
  }

  // Updates the active sessions for the given aggregate category group.
  // Return false if the group is NULL.
  // Thread safe. Multiple threads can call simultaneously for same index.
  static bool UpdateGroupCategory(size_t group_index) {
    struct percetto_category* group_category =
        s_percetto.groups[group_index].load(std::memory_order_relaxed);
    // A NULL slot signifies the end of the array. It's ok to race
    // with the adding of group categories.
    if (!group_category)
      return false;

    uint32_t new_sessions = 0;
    for (uint32_t i = 0; i < group_category->group.count; ++i) {
      uint8_t cat_index = group_category->group.child_ids[i];
      assert(cat_index >= 0 && cat_index < s_percetto.category_count);
      new_sessions |= s_percetto.categories[cat_index]->sessions;
    }
    group_category->sessions = new_sessions;
    return true;
  }

  // Updates the active sessions for all aggregate category groups.
  // Thread safe.
  static void UpdateGroupCategories() {
    // Now go through dynamic category groups and enable them if any of the
    // corresponding individual categories are enabled.
    // ie: group->sessions = (child1->sessions | child2->sessions);
    for (size_t i = 0; i < s_percetto.groups.max_size(); ++i) {
      if (!UpdateGroupCategory(i))
        return;
    }
  }

  static bool Register() {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("track_event");

    protozero::HeapBuffered<TrackEventDescriptor> ted;
    for (int i = 0; i < s_percetto.category_count; i++) {
      auto cat = ted->add_available_categories();
      cat->set_name(s_percetto.categories[i]->name);
      cat->set_description(s_percetto.categories[i]->description);
      for (const char* tag : s_percetto.categories[i]->tags) {
        if (tag)
          cat->add_tags(tag);
      }
    }
    dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

    return Base::Register(dsd);
  }

  // Thread safe.
  static inline void TraceTrackEvent(
      struct percetto_category* category,
      const uint32_t sessions,
      const TrackEvent::Type type,
      const char* name,
      uint64_t timestamp,
      uint64_t track_uuid,
      int64_t extra,
      const struct percetto_event_extended* extended) {
    bool do_once = NeedIncrementalUpdate();
    TraceWithInstances(sessions, [&](Base::TraceContext ctx) {
      if (PERCETTO_UNLIKELY(do_once))
        OncePerTraceSession(ctx);

      /* TODO incremental state */
      auto packet = NewTracePacket(ctx,
          TracePacket::SEQ_NEEDS_INCREMENTAL_STATE, timestamp);

      auto event = packet->set_track_event();
      event->set_type(type);

      if (PERCETTO_UNLIKELY(track_uuid))
        event->set_track_uuid(perfetto::Track(track_uuid).uuid);

      /* TODO intern strings with EventCategory */

      if (PERCETTO_LIKELY(category->name)) {
        event->add_categories(category->name, strlen(category->name));
      } else {
        AddCategoryGroup(event, category);
      }

      if (type == TrackEvent::Type::TrackEvent_Type_TYPE_COUNTER) {
        event->set_counter_value(extra);
      } else {
        if (type != TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END)
          event->set_name(name, strlen(name));
        if (extra != 0)
          event->add_flow_ids(static_cast<uint64_t>(extra));
      }
      while (PERCETTO_UNLIKELY(extended)) {
        AddExtendedData(event, extended);
        extended = extended->next;
      }
    });
  }

 private:
  static void AddCategoryGroup(
      TrackEvent* event, const struct percetto_category* category) {
    for (uint32_t i = 0; i < category->group.count; ++i) {
      uint8_t cat_index = category->group.child_ids[i];
      assert(cat_index >= 0 && cat_index < s_percetto.category_count);
      const char* name = s_percetto.categories[cat_index]->name;
      event->add_categories(name, strlen(name));
    }
  }

  static void AddExtendedData(
      TrackEvent* event, const struct percetto_event_extended* extended) {
    switch(extended->type) {
      case PERCETTO_EVENT_EXTENDED_DEBUG_DATA: {
        AddDebugData(event,
            reinterpret_cast<const struct percetto_event_debug_data*>(
                extended));
        break;
      }
    }
  }

  static void AddDebugData(
      TrackEvent* event, const struct percetto_event_debug_data* data) {
    auto debug = event->add_debug_annotations();
    debug->set_name(data->name);
    switch(data->type) {
      case PERCETTO_EVENT_DEBUG_DATA_BOOL:
        debug->set_bool_value(!!data->bool_value);
        break;
      case PERCETTO_EVENT_DEBUG_DATA_UINT:
        debug->set_uint_value(data->uint_value);
        break;
      case PERCETTO_EVENT_DEBUG_DATA_INT:
        debug->set_int_value(data->int_value);
        break;
      case PERCETTO_EVENT_DEBUG_DATA_DOUBLE:
        debug->set_double_value(data->double_value);
        break;
      case PERCETTO_EVENT_DEBUG_DATA_STRING:
        debug->set_string_value(data->string_value,
                                strlen(data->string_value));
        break;
      case PERCETTO_EVENT_DEBUG_DATA_POINTER:
        debug->set_pointer_value(data->pointer_value);
        break;
    }
  }

  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags,
      uint64_t timestamp) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(timestamp);
    packet->set_sequence_flags(seq_flags);
    // Trace processor may not understand trace defaults yet, so we do this.
    if (PERCETTO_UNLIKELY(s_percetto.perfetto_clock !=
        perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME))
      packet->set_timestamp_clock_id(s_percetto.perfetto_clock);

    return packet;
  }

  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags) {
    return NewTracePacket(ctx, seq_flags, GetTimestampNs());
  }

  // Thread safe. Returns whether the calling thread needs to update
  // incremental perfetto state and syncs to s_target_incremental_update.
  static inline bool NeedIncrementalUpdate() {
    int target_update = s_target_incremental_update.load(
        std::memory_order_relaxed);
    return s_committed_incremental_update != target_update;
  }

  static inline void SetDoingIncrementalUpdate() {
    s_committed_incremental_update = s_target_incremental_update.load(
        std::memory_order_acquire);
  }

  static void OncePerTraceSession(Base::TraceContext& ctx) {
    SetDoingIncrementalUpdate();
    auto default_track = perfetto::ThreadTrack::Current();

    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
      auto defaults = packet->set_trace_packet_defaults();
      defaults->set_timestamp_clock_id(s_percetto.perfetto_clock);

      auto track_defaults = defaults->set_track_event_defaults();
      track_defaults->set_track_uuid(default_track.uuid);
    }

    // Add process track (happens for every thread, but that's ok).
    {
      auto process_track = perfetto::ProcessTrack::Current();
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
      process_track.Serialize(packet->set_track_descriptor());
    }

    // Add thread track.
    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
      default_track.Serialize(packet->set_track_descriptor());
    }

    {
      // Add custom tracks (ie: for counters)
      for (size_t i = 0; i < s_percetto.tracks.max_size(); ++i) {
        struct percetto_track* track =
            s_percetto.tracks[i].load(std::memory_order_relaxed);
        // The first NULL slot signifies the end of the array. It's ok to race
        // with the adding of tracks.
        if (!track)
          break;

        auto packet =
            NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
        perfetto::Track perfetto_track(track->uuid);
        auto track_descriptor = packet->set_track_descriptor();
        perfetto_track.Serialize(track_descriptor);
        track_descriptor->set_name(track->name);
        if (static_cast<percetto_track_type>(track->type) ==
            PERCETTO_TRACK_COUNTER)
          track_descriptor->set_counter();
      }

    }
  }
};

}  // anonymous namespace

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);

extern "C"
int percetto_init(size_t category_count,
                  struct percetto_category** categories,
                  enum percetto_clock clock_id) {
  if (s_percetto.is_initialized) {
    fprintf(stderr, "%s error: already initialized\n", __func__);
    return -2;
  }
  if (category_count > PERCETTO_MAX_CATEGORIES) {
    fprintf(stderr, "%s error: too many categories\n", __func__);
    return -3;
  }

  clockid_t system_clock = 0;
  BuiltinClock perfetto_clock = perfetto::protos::pbzero::BUILTIN_CLOCK_UNKNOWN;
  if (clock_id == PERCETTO_CLOCK_DONT_CARE)
    system_clock = DetermineClockId(&perfetto_clock);
  else
    system_clock = GetClockIdFrom(static_cast<BuiltinClock>(clock_id),
                                  &perfetto_clock);
  if (!CheckSystemClock(system_clock)) {
    fprintf(stderr, "%s error: system clock error\n", __func__);
    return -4;
  }

  s_percetto.is_initialized = 1;
  s_percetto.categories = categories;
  s_percetto.category_count = category_count;
  s_percetto.trace_clock_id = system_clock;
  s_percetto.perfetto_clock = perfetto_clock;

  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  return PercettoDataSource::Register() ? 0 : -1;
}

extern "C"
int percetto_register_group_category(struct percetto_category* category) {
  // Lock-free add to array.
  for (size_t i = 0; i < s_percetto.groups.max_size(); ++i) {
    struct percetto_category* null_category = NULL;
    // Try to swap new category into this slot.
    if (s_percetto.groups[i].compare_exchange_strong(null_category, category)) {
      // Update the trace enabled state for this aggregate category.
      PercettoDataSource::UpdateGroupCategory(i);
      return 0;
    }
  }

  fprintf(stderr, "%s error: no more group categories are allowed\n",
          __func__);
  return -1;
}

extern "C"
int percetto_register_track(struct percetto_track* track) {
  // Lock-free add to array.
  for (size_t i = 0; i < s_percetto.tracks.max_size(); ++i) {
    struct percetto_track* null_track = NULL;
    // Try to swap new track into this slot.
    if (s_percetto.tracks[i].compare_exchange_strong(null_track, track)) {
      // Update incremental state to send info about this track.
      PercettoDataSource::KickIncrementalUpdates();
      return 0;
    }
  }

  fprintf(stderr, "%s error: no more tracks are allowed\n", __func__);
  return -1;
}

extern "C"
void percetto_event_begin(struct percetto_category* category,
                          uint32_t sessions,
                          const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN,
      name, GetTimestampNs(), 0, 0, NULL);
}

extern "C"
void percetto_event_end(struct percetto_category* category,
                        uint32_t sessions) {
  PercettoDataSource::TraceTrackEvent(
      category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
      NULL, GetTimestampNs(), 0, 0, NULL);
}

extern "C"
void percetto_event(struct percetto_category* category,
                    uint32_t sessions,
                    int32_t type,
                    const struct percetto_event_data* data) {
  TrackEvent::Type perfetto_type = static_cast<TrackEvent::Type>(type);
  uint64_t timestamp;
  if (PERCETTO_LIKELY(data->timestamp))
    timestamp = data->timestamp;
  else
    timestamp = GetTimestampNs();
  if (perfetto_type == TrackEvent::Type::TrackEvent_Type_TYPE_INSTANT &&
      data->extra != 0) {
    // TODO(jbates): remove when perfetto UI bug fixed for INSTANT+FLOW events.
    // Perfetto UI bug requires flow events to be begin+end pairs.
    PercettoDataSource::TraceTrackEvent(
        category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN,
        data->name, timestamp, data->track_uuid, data->extra, NULL);
    PercettoDataSource::TraceTrackEvent(
        category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
        data->name, timestamp + 1, data->track_uuid, 0, NULL);
  } else {
    PercettoDataSource::TraceTrackEvent(
        category, sessions, perfetto_type, data->name, timestamp,
        data->track_uuid, data->extra, NULL);
  }
}

extern "C"
void percetto_event_extended(struct percetto_category* category,
                             uint32_t sessions,
                             int32_t type,
                             const struct percetto_event_data* data,
                             const struct percetto_event_extended* extended) {
  TrackEvent::Type perfetto_type = static_cast<TrackEvent::Type>(type);
  uint64_t timestamp;
  if (PERCETTO_LIKELY(data->timestamp))
    timestamp = data->timestamp;
  else
    timestamp = GetTimestampNs();
  if (perfetto_type == TrackEvent::Type::TrackEvent_Type_TYPE_INSTANT &&
      data->extra != 0) {
    // TODO(jbates): remove when perfetto UI bug fixed for INSTANT+FLOW events.
    // Perfetto UI bug requires flow events to be begin+end pairs.
    PercettoDataSource::TraceTrackEvent(
        category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN,
        data->name, timestamp, data->track_uuid, data->extra, extended);
    PercettoDataSource::TraceTrackEvent(
        category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
        data->name, timestamp + 1, data->track_uuid, 0, extended);
  } else {
    PercettoDataSource::TraceTrackEvent(
        category, sessions, perfetto_type, data->name, timestamp,
        data->track_uuid, data->extra, extended);
  }
}
