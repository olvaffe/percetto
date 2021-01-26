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

#include <atomic>
#include <perfetto.h>

#include "perfetto-port.h"

namespace {

using perfetto::protos::gen::TrackDescriptor;
using perfetto::protos::gen::TrackEventConfig;
using perfetto::protos::pbzero::CounterDescriptor_Unit_UNIT_COUNT;
using perfetto::protos::pbzero::DataSourceDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDescriptor;

struct Percetto {
  int is_initialized;
  perfetto::base::PlatformThreadId init_thread;
  struct percetto_category** categories;
  struct percetto_track* tracks[PERCETTO_MAX_TRACKS];
  int category_count;
  int track_count;
  int trace_session;
  clockid_t trace_clock_id;
};

static Percetto s_percetto;

static clockid_t DetermineSystemClockId() {
  // Determine clock to use (follows perfetto's preference for BOOTTIME).
  struct timespec ts = {};
  int result = clock_gettime(CLOCK_BOOTTIME, &ts);
  return (result == 0 ? CLOCK_BOOTTIME : CLOCK_MONOTONIC);
}

static inline perfetto::protos::pbzero::BuiltinClock GetPerfettoClockId() {
  return s_percetto.trace_clock_id == CLOCK_MONOTONIC ?
    perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC :
    perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME;
}

static inline uint64_t GetTimestampNs() {
  struct timespec ts = {};
  clock_gettime(s_percetto.trace_clock_id, &ts);
  return static_cast<uint64_t>(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

class PercettoDataSource : public perfetto::DataSource<PercettoDataSource> {
  using Base = DataSource<PercettoDataSource>;

 public:
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
      if (IsCategoryEnabled(*s_percetto.categories[i], config)) {
        std::atomic_fetch_or(&s_percetto.categories[i]->sessions,
            1 << args.internal_instance_index);
      }
    }
    ++s_percetto.trace_session;
  }

  void OnStart(const DataSourceBase::StartArgs&) override {}

  void OnStop(const DataSourceBase::StopArgs& args) override {
    for (int i = 0; i < s_percetto.category_count; i++) {
      std::atomic_fetch_and(&s_percetto.categories[i]->sessions,
          ~(1 << args.internal_instance_index));
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
      if (s_percetto.categories[i]->flags & PERCETTO_CATEGORY_FLAG_SLOW)
        cat->add_tags("slow");
      if (s_percetto.categories[i]->flags & PERCETTO_CATEGORY_FLAG_DEBUG)
        cat->add_tags("debug");
    }
    dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

    return Base::Register(dsd);
  }

  static inline void TraceTrackEvent(struct percetto_category* category,
                                     const uint32_t sessions,
                                     const TrackEvent::Type type,
                                     const char* name,
                                     uint64_t timestamp,
                                     uint64_t track_uuid,
                                     int64_t extra) {
    bool do_once = NeedToSendTraceConfig();
    TraceWithInstances(sessions, [&](Base::TraceContext ctx) {
      if (PERCETTO_UNLIKELY(do_once))
        OncePerTraceSession(ctx);

      /* TODO incremental state */
      auto packet = NewTracePacket(ctx, timestamp,
          TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto event = packet->set_track_event();
      event->set_type(type);

      /* TODO intern strings with EventCategory */
      event->add_categories(category->name, strlen(category->name));
      if (type == TrackEvent::Type::TrackEvent_Type_TYPE_COUNTER) {
        // TODO(jbates): set track uuid on other thread events to
        //               perfetto::ThreadTrack::Current().uuid
        event->set_track_uuid(perfetto::Track(track_uuid).uuid);
        event->set_counter_value(extra);
      } else if (type != TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END) {
        event->set_name(name, strlen(name));
        if (extra != 0)
          event->add_flow_ids(static_cast<uint64_t>(extra));
      }
    });
  }

 private:
  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags,
      uint64_t timestamp) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(timestamp);
    packet->set_sequence_flags(seq_flags);
    // Trace processor may not understand trace defaults yet, so we do this.
    if (PERCETTO_UNLIKELY(GetPerfettoClockId() !=
        perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME))
      packet->set_timestamp_clock_id(GetPerfettoClockId());

    return packet;
  }

  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags) {
    return NewTracePacket(ctx, seq_flags, GetTimestampNs());
  }

  static bool NeedToSendTraceConfig() {
    // TODO(jbates): refactor this out of the per-event calls for better perf.
    // This is per-thread
    static thread_local int session = 0;
    if (PERCETTO_LIKELY(session == s_percetto.trace_session))
      return false;
    session = s_percetto.trace_session;
    return true;
  }

  static void OncePerTraceSession(Base::TraceContext& ctx) {
    auto default_track = perfetto::ThreadTrack::Current();

    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
      auto defaults = packet->set_trace_packet_defaults();
      defaults->set_timestamp_clock_id(GetPerfettoClockId());

      auto track_defaults = defaults->set_track_event_defaults();
      track_defaults->set_track_uuid(default_track.uuid);
    }

    // Add process track.
    if (perfetto::base::GetThreadId() == s_percetto.init_thread) {
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
      for (int i = 0; i < s_percetto.track_count; ++i) {
        auto packet =
            NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
        perfetto::Track perfetto_track(s_percetto.tracks[i]->uuid);
        auto track_descriptor = packet->set_track_descriptor();
        perfetto_track.Serialize(track_descriptor);
        track_descriptor->set_name(s_percetto.tracks[i]->name);
        track_descriptor->set_counter();
      }
    }
  }
};

}  // anonymous namespace

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);

int percetto_init(size_t category_count,
                  struct percetto_category** categories) {
  if (s_percetto.is_initialized) {
    fprintf(stderr, "error: percetto is already initialized\n");
    return -2;
  }
  s_percetto.is_initialized = 1;
  s_percetto.init_thread = perfetto::base::GetThreadId();
  s_percetto.categories = categories;
  s_percetto.category_count = category_count;
  s_percetto.track_count = 0;
  s_percetto.trace_session = 0;
  s_percetto.trace_clock_id = DetermineSystemClockId();

  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  return PercettoDataSource::Register() ? 0 : -1;
}

int percetto_register_track(struct percetto_track* track) {
  if (s_percetto.track_count == PERCETTO_MAX_TRACKS) {
    fprintf(stderr, "error: no more tracks are allowed\n");
    return -1;
  }
  s_percetto.tracks[s_percetto.track_count++] = track;
  return 0;
}

void percetto_event_begin(struct percetto_category* category,
                          uint32_t sessions,
                          const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN,
      name, GetTimestampNs(), 0, 0);
}

void percetto_event_end(struct percetto_category* category,
                        uint32_t sessions) {
  PercettoDataSource::TraceTrackEvent(
      category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
      NULL, GetTimestampNs(), 0, 0);
}

void percetto_event(struct percetto_category* category,
                    uint32_t sessions,
                    const char* name,
                    enum percetto_event_type type,
                    uint64_t track_uuid,
                    int64_t extra) {
  TrackEvent::Type perfetto_type = static_cast<TrackEvent::Type>(type);
  PercettoDataSource::TraceTrackEvent(
      category, sessions, perfetto_type,
      name, GetTimestampNs(), track_uuid, extra);
}
