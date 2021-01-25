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

using perfetto::protos::gen::TrackEventConfig;
using perfetto::protos::pbzero::DataSourceDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDescriptor;

struct Percetto {
  int is_initialized;
  perfetto::base::PlatformThreadId init_thread;
  struct percetto_category** categories;
  int category_count;
  int trace_session;
};

static Percetto s_percetto;

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

  static void TraceTrackEvent(struct percetto_category* category,
                              uint32_t instance_mask,
                              TrackEvent::Type type,
                              const char* name) {
    bool do_once = NeedToSendTraceConfig();
    TraceWithInstances(instance_mask, [&](Base::TraceContext ctx) {
      if (PERCETTO_UNLIKELY(do_once))
        OncePerTraceSession(ctx);

      /* TODO incremental state */
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto event = packet->set_track_event();
      event->set_type(type);

      /* TODO intern strings with EventCategory */
      event->add_categories(category->name, strlen(category->name));
      event->set_name(name, strlen(name));
    });
  }

  static void TraceTrackEventEnd(struct percetto_category* category,
                                 uint32_t instance_mask) {
    bool do_once = NeedToSendTraceConfig();
    TraceWithInstances(instance_mask, [&](Base::TraceContext ctx) {
      if (PERCETTO_UNLIKELY(do_once))
        OncePerTraceSession(ctx);

      /* TODO incremental state */
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto event = packet->set_track_event();
      event->set_type(TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END);

      /* TODO intern strings with EventCategory */
      event->add_categories(category->name, strlen(category->name));
    });
  }

 private:
  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(
        static_cast<uint64_t>(perfetto::base::GetWallTimeNs().count()));
    packet->set_sequence_flags(seq_flags);

    return packet;
  }

  static bool NeedToSendTraceConfig() {
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
      defaults->set_timestamp_clock_id(
          perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);

      auto track_defaults = defaults->set_track_event_defaults();
      track_defaults->set_track_uuid(default_track.uuid);
    }

    /* TODO see TrackRegistry */
    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
      default_track.Serialize(packet->set_track_descriptor());
    }

    if (perfetto::base::GetThreadId() == s_percetto.init_thread) {
      auto process_track = perfetto::ProcessTrack::Current();
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
      process_track.Serialize(packet->set_track_descriptor());
    }
  }
};

}  // anonymous namespace

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);

bool percetto_init(size_t category_count,
                   struct percetto_category** categories) {
  if (s_percetto.is_initialized) {
    fprintf(stderr, "error: percetto is already initialized\n");
    return false;
  }
  s_percetto.is_initialized = 1;
  s_percetto.init_thread = perfetto::base::GetThreadId();
  s_percetto.categories = categories;
  s_percetto.category_count = category_count;
  s_percetto.trace_session = 0;

  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  return PercettoDataSource::Register();
}

void percetto_slice_begin(struct percetto_category* category,
                          uint32_t instance_mask,
                          const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, instance_mask,
      TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN, name);
}

void percetto_slice_end(struct percetto_category* category,
                        uint32_t instance_mask) {
  PercettoDataSource::TraceTrackEventEnd(
      category, instance_mask);
}

void percetto_instant(struct percetto_category* category,
                      uint32_t instance_mask,
                      const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, instance_mask, TrackEvent::Type::TrackEvent_Type_TYPE_INSTANT,
      name);
}
