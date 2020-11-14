/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "percetto.h"

#include <perfetto.h>

namespace {

using perfetto::protos::pbzero::DataSourceDescriptor;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDescriptor;

struct Percetto {
  const char** categories;
  int category_count;
  percetto_category_state_callback callback;
  void* callback_data;
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
      /* TODO incremental state */
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(
          static_cast<uint64_t>(perfetto::base::GetWallTimeNs().count()));
      packet->set_timestamp_clock_id(
          perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);
      packet->set_sequence_flags(
          perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto event = packet->set_track_event();
      event->set_type(type);

      /* TODO intern strings */
      event->add_categories(sPercetto.categories[category]);
      if (name)
        event->set_name(name);
    });
  }

 private:
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);

}  // anonymous namespace

bool percetto_init(int category_count,
                   const char** categories,
                   percetto_category_state_callback callback,
                   void* callback_data) {
  sPercetto.categories = categories;
  sPercetto.category_count = category_count;
  sPercetto.callback = callback;
  sPercetto.callback_data = callback_data;

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
