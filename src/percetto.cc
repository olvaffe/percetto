/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "percetto.h"

#include <perfetto.h>

namespace {

using perfetto::protos::pbzero::DataSourceDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDescriptor;

struct Percetto {
  perfetto::base::PlatformThreadId init_thread;
  const char** categories;
  int category_count;
  percetto_category_state_callback callback;
  void* callback_data;
  int trace_session;
};

Percetto sPercetto;

class PercettoDataSource : public perfetto::DataSource<PercettoDataSource> {
  using Base = DataSource<PercettoDataSource>;

 public:
  void OnSetup(const DataSourceBase::SetupArgs&) override {
    // TODO follow TrackEventInternal::IsCategoryEnabled
  }

  void OnStart(const DataSourceBase::StartArgs& args) override {
    for (int i = 0; i < sPercetto.category_count; i++) {
      sPercetto.callback(i, args.internal_instance_index,
                         PERCETTO_CATEGORY_STATE_START,
                         sPercetto.callback_data);
    }
    ++sPercetto.trace_session;
  }

  void OnStop(const DataSourceBase::StopArgs& args) override {
    for (int i = 0; i < sPercetto.category_count; i++) {
      sPercetto.callback(i, args.internal_instance_index,
                         PERCETTO_CATEGORY_STATE_STOP, sPercetto.callback_data);
    }
  }

  static bool Register() {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("track_event");

    protozero::HeapBuffered<TrackEventDescriptor> ted;
    for (int i = 0; i < sPercetto.category_count; i++) {
      auto cat = ted->add_available_categories();
      cat->set_name(sPercetto.categories[i]);
    }
    dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

    return Base::Register(dsd);
  }

  static void TraceTrackEvent(int category,
                              uint32_t instance_mask,
                              TrackEvent::Type type,
                              const char* name) {
    TraceWithInstances(instance_mask, [&](Base::TraceContext ctx) {
      OncePerTraceSession(ctx);

      /* TODO incremental state */
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto event = packet->set_track_event();
      event->set_type(type);

      /* TODO intern strings */
      event->add_categories(sPercetto.categories[category]);
      if (name)
        event->set_name(name);
    });
  }

 private:
  static protozero::MessageHandle<TracePacket> NewTracePacket(
      Base::TraceContext& ctx,
      uint32_t seq_flags) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(
        static_cast<uint64_t>(perfetto::base::GetWallTimeNs().count()));
    packet->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);
    packet->set_sequence_flags(seq_flags);

    return packet;
  }

  static void OncePerTraceSession(Base::TraceContext& ctx) {
    // this is per-thread; use TLS
    static thread_local int session = 0;
    if (session == sPercetto.trace_session)
      return;
    session = sPercetto.trace_session;

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

    if (perfetto::base::GetThreadId() == sPercetto.init_thread) {
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

bool percetto_init(int category_count,
                   const char** categories,
                   percetto_category_state_callback callback,
                   void* callback_data) {
  sPercetto.init_thread = perfetto::base::GetThreadId();
  sPercetto.categories = categories;
  sPercetto.category_count = category_count;
  sPercetto.callback = callback;
  sPercetto.callback_data = callback_data;
  sPercetto.trace_session = 0;

  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  return PercettoDataSource::Register();
}

void percetto_slice_begin(int category,
                          uint32_t instance_mask,
                          const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, instance_mask,
      TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_BEGIN, name);
}

void percetto_slice_end(int category, uint32_t instance_mask) {
  PercettoDataSource::TraceTrackEvent(
      category, instance_mask, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
      nullptr);
}

void percetto_instant(int category, uint32_t instance_mask, const char* name) {
  PercettoDataSource::TraceTrackEvent(
      category, instance_mask, TrackEvent::Type::TrackEvent_Type_TYPE_INSTANT,
      name);
}
