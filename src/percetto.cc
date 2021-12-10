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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for pthread_getname_np
#endif

#if defined(_M_IA64) || defined(_M_IX86) || defined(__ia64__) || \
    defined(__i386__) || defined(__amd64__) || defined(__x86_64__) || \
    defined(_M_AMD64)
  #define HAS_RDTSC
#endif
#if defined(__aarch64__)
  #define HAS_CNTVCT
#endif

#include "percetto.h"

#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAS_RDTSC
  #ifdef _MSC_VER
  #include <intrin.h>
  #else
  #include <x86intrin.h>
  #endif
#endif

#include <array>
#include <atomic>
#include <cstdlib>
#include <mutex>

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
  std::mutex lock;
  int is_initialized;
  std::array<struct percetto_category*, PERCETTO_MAX_CATEGORIES> categories;
  std::atomic<int32_t> category_count;
  std::array<std::atomic<struct percetto_category*>,
             PERCETTO_MAX_GROUP_CATEGORIES> groups;
  std::array<std::atomic<struct percetto_track*>, PERCETTO_MAX_TRACKS> tracks;
  clockid_t trace_clock_id;
  BuiltinClock perfetto_clock;
  perfetto::base::PlatformProcessId process_pid;
  uint64_t process_uuid;
};

// Thread tracks use kernel tid (pid_t) for their track ID, so to make sure
// that custom tracks have unique IDs we need to offset by maximum thread ID
// value. Unfortunately there does not seem to be a portable way to get that
// constant, so we will use a large value.
constexpr uint64_t kCustomTrackIdOffset = 1ull << 32;

// Perfetto's protobuf protocol documents 64-127 as the custom clock id range:
// https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/protos/perfetto/trace/clock_snapshot.proto#43
// This ID will be used in a clock_snapshot proto to represent the CPU counter
// value along with a system clock value for synchronization.
constexpr uint32_t kCpuCounterClockId = 64;

static Percetto s_percetto;

static inline bool IsGroupCategory(const struct percetto_category* category) {
  return category->ext->name == NULL;
}

static inline uint64_t FnvHashBegin() {
  return 14695981039346656037ull;
}

static uint64_t FnvHashAdd(uint64_t hash, const void* data, size_t data_len) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
  const uint8_t* bytes_end = bytes + data_len;
  for (; bytes != bytes_end; ++bytes) {
    hash *= 1099511628211ull;
    hash ^= *bytes;
  }
  return hash;
}

template<typename T> void FnvHashAdd(uint64_t* hash, const T* data) {
  *hash = FnvHashAdd(*hash, static_cast<const void*>(data), sizeof(T));
}

static uint64_t GetProcessUuid() {
  // The process UUID is a hash of the namespace ID and PID.
  char path[64];
  int32_t pid = static_cast<int32_t>(s_percetto.process_pid);
  snprintf(path, sizeof(path), "/proc/%d/ns/pid", pid);

  uint64_t uuid = FnvHashBegin();
  FnvHashAdd(&uuid, &s_percetto.process_pid);

  struct stat statbuf;
  int result = stat(path, &statbuf);
  if (result == 0) {
    FnvHashAdd(&uuid, &statbuf.st_ino);
  } else {
    fprintf(stderr, "%s: stat error: %d\n", __func__, errno);
  }

  return uuid;
}

static const char* TryGetProcessExeName(char* buffer, size_t buffer_size) {
  char path[64];
  int32_t pid = static_cast<int32_t>(s_percetto.process_pid);
  snprintf(path, sizeof(path), "/proc/%d/exe", pid);

  ssize_t result = readlink(path, buffer, buffer_size);
  if (result < 0) {
    return NULL;
  }

  if (result < static_cast<ssize_t>(buffer_size)) {
    buffer[result] = '\0';
  } else {
    buffer[buffer_size - 1] = '\0';
  }

  return buffer;
}

static const char* TryGetThreadName(char* buffer, size_t buffer_size) {
  pthread_t thread = pthread_self();
  int result = pthread_getname_np(thread, buffer, buffer_size);
  if (result != 0)
    return NULL;
  return buffer;
}

static uint64_t GetTrackUuid(uint64_t trackid) {
  return trackid ^ s_percetto.process_uuid;
}

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

static BuiltinClock GetBuiltinClockIdFrom(clockid_t clockid) {
  switch(clockid) {
    default:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME;
    case CLOCK_REALTIME_COARSE:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME_COARSE;
    case CLOCK_MONOTONIC:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC;
    case CLOCK_MONOTONIC_COARSE:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_COARSE;
    case CLOCK_MONOTONIC_RAW:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC_RAW;
    case CLOCK_BOOTTIME:
      return perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME;
  }
}

static inline uint64_t GetCpuTicks() {
#ifdef HAS_RDTSC
  return __rdtsc();
#elif defined(HAS_CNTVCT)
  uint64_t vct;
  asm volatile("mrs %0, cntvct_el0" : "=r"(vct));
  return vct;
#else
  return 0;
#endif
}

static inline uint64_t GetTimestampNs() {
  struct timespec ts = {};
  clock_gettime(s_percetto.trace_clock_id, &ts);
  return static_cast<uint64_t>(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

static uint32_t GetEnvU32(const char* var_name, uint32_t default_value) {
  const char* var = std::getenv(var_name);
  if (var) {
    long long value = std::atoll(var);
    if (value >= 0 && value <= UINT_MAX)
      return static_cast<uint32_t>(value);
  }
  return default_value;
}

struct PercettoIncrementalState {
  bool need_update = true;
};

struct PercettoDataSourceTraits : public perfetto::DefaultDataSourceTraits {
  using IncrementalStateType = PercettoIncrementalState;
};

class PercettoDataSource
    : public perfetto::DataSource<PercettoDataSource,
                                  PercettoDataSourceTraits> {
  using Base = DataSource<PercettoDataSource, PercettoDataSourceTraits>;

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
    int count = s_percetto.category_count.load(std::memory_order_acquire);
    for (int i = 0; i < count; i++) {
      std::array<const char*, PERCETTO_MAX_CATEGORY_TAGS> tags;
      // Tags are all strings except the first which is description:
      std::copy(std::begin(s_percetto.categories[i]->ext->strings) + 1,
                std::end(s_percetto.categories[i]->ext->strings),
                std::begin(tags));
      if (IsCategoryEnabled(s_percetto.categories[i]->ext->name, tags, config)) {
        std::atomic_fetch_or(&s_percetto.categories[i]->sessions,
                             1ul << args.internal_instance_index);
      }
    }
    UpdateGroupCategories();
  }

  void OnStart(const DataSourceBase::StartArgs&) override {}

  void OnStop(const DataSourceBase::StopArgs& args) override {
    int count = s_percetto.category_count.load(std::memory_order_acquire);
    for (int i = 0; i < count; i++) {
      std::atomic_fetch_and(&s_percetto.categories[i]->sessions,
          ~(1ul << args.internal_instance_index));
    }
    UpdateGroupCategories();

    Trace([](PercettoDataSource::TraceContext ctx) {
      auto packet = ctx.NewTracePacket();
      packet->Finalize();
      ctx.Flush();
    });
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
    for (auto category : group_category->ext->group) {
      if (category)
        new_sessions |= category->sessions.load(std::memory_order_acquire);
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

  static bool Register(const struct percetto_category* const* begin,
                       const struct percetto_category* const* end,
                       bool is_first_init) {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("track_event");

    protozero::HeapBuffered<TrackEventDescriptor> ted;
    const struct percetto_category* const* pc;
    for (pc = begin; pc != end; ++pc) {
      auto cat = ted->add_available_categories();
      cat->set_name((*pc)->ext->name);
      cat->set_description((*pc)->ext->strings[0]);
      // Tags are all strings except the first which is description:
      for (auto tag = std::begin((*pc)->ext->strings) + 1;
           tag != std::end((*pc)->ext->strings); ++tag) {
        if (*tag)
          cat->add_tags(*tag);
      }
    }
    dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

    if (is_first_init) {
      return Base::Register(dsd);
    } else {
      Base::UpdateDescriptor(dsd);
      return true;
    }
  }

  // Thread safe.
  static inline void TraceTrackEvent(
      const struct percetto_category* category,
      const uint32_t sessions,
      const TrackEvent::Type type,
      const char* name,
      uint64_t timestamp,
      const struct percetto_track* track,
      int64_t extra,
      const struct percetto_event_extended* extended) {
    TraceWithInstances(sessions, [&](Base::TraceContext ctx) {
      if (PERCETTO_UNLIKELY(ctx.GetIncrementalState()->need_update))
        DoIncrementalUpdate(ctx);

      auto packet = NewTracePacket(ctx,
          TracePacket::SEQ_NEEDS_INCREMENTAL_STATE, timestamp);

      auto event = packet->set_track_event();
      event->set_type(type);

      if (PERCETTO_UNLIKELY(track))
        event->set_track_uuid(track->uuid.load(std::memory_order_relaxed));

      if (PERCETTO_LIKELY(category->name_iid)) {
        event->add_category_iids(category->name_iid);
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
    for (auto category : category->ext->group) {
      if (category)
        event->add_category_iids(category->name_iid);
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

  static void DoIncrementalUpdate(Base::TraceContext& ctx) {
    ctx.GetIncrementalState()->need_update = false;
    auto tid = perfetto::base::GetThreadId();
    uint64_t thread_track_uuid = GetTrackUuid(static_cast<uint64_t>(tid));

    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
      auto defaults = packet->set_trace_packet_defaults();
      defaults->set_timestamp_clock_id(s_percetto.perfetto_clock);

      auto track_defaults = defaults->set_track_event_defaults();
      track_defaults->set_track_uuid(thread_track_uuid);

      auto interned = packet->set_interned_data();
      int count = s_percetto.category_count.load(std::memory_order_acquire);
      for (int i = 0; i < count; i++) {
        auto cat = interned->add_event_categories();
        cat->set_name(s_percetto.categories[i]->ext->name);
        cat->set_iid(s_percetto.categories[i]->name_iid);
      }
    }

    // Add process track (happens for every thread, but that's ok).
    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto track_descriptor = packet->set_track_descriptor();
      track_descriptor->set_uuid(s_percetto.process_uuid);

      auto process = track_descriptor->set_process();
      process->set_pid(static_cast<int32_t>(s_percetto.process_pid));
      char buffer[128];
      const char* name = TryGetProcessExeName(buffer, sizeof(buffer));
      if (name)
        process->set_process_name(name, strlen(name));
    }

    // Add thread track.
    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto track_descriptor = packet->set_track_descriptor();
      track_descriptor->set_uuid(thread_track_uuid);
      track_descriptor->set_parent_uuid(s_percetto.process_uuid);

      auto thread_descriptor = track_descriptor->set_thread();
      thread_descriptor->set_pid(static_cast<int32_t>(s_percetto.process_pid));
      thread_descriptor->set_tid(static_cast<int32_t>(tid));
      char buffer[128];
      const char* name = TryGetThreadName(buffer, sizeof(buffer));
      if (name)
        thread_descriptor->set_thread_name(name, strlen(name));
    }

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
      auto track_descriptor = packet->set_track_descriptor();
      track_descriptor->set_uuid(
          track->uuid.load(std::memory_order_relaxed));
      track_descriptor->set_parent_uuid(
          track->parent_uuid.load(std::memory_order_relaxed));
      track_descriptor->set_name(track->name);
      if (static_cast<percetto_track_type>(track->type) ==
          PERCETTO_TRACK_COUNTER)
        track_descriptor->set_counter();
    }

    // Add time sync data
    {
      auto packet =
          NewTracePacket(ctx, TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

      auto clocks = packet->set_clock_snapshot();

      uint64_t boottime = GetTimestampNs();
      uint64_t cputime = GetCpuTicks();
      // Read again to make sure we get times without cache misses.
      boottime = GetTimestampNs();
      cputime = GetCpuTicks();

      auto clock_boottime = clocks->add_clocks();
      clock_boottime->set_clock_id(
          GetBuiltinClockIdFrom(s_percetto.trace_clock_id));
      clock_boottime->set_timestamp(boottime);

      auto clock_cputime = clocks->add_clocks();
      clock_cputime->set_clock_id(kCpuCounterClockId);
      clock_cputime->set_timestamp(cputime);
    }
  }
};

static inline size_t add_group_category(struct percetto_category* category) {
  // Lock-free add to array.
  // Fence so that previous writes to *category complete before adding
  // the category below.
  std::atomic_thread_fence(std::memory_order_release);
  for (size_t i = 0; i < s_percetto.groups.max_size(); ++i) {
    struct percetto_category* null_category = NULL;
    // Try to swap new category into this slot.
    if (s_percetto.groups[i].compare_exchange_strong(null_category, category)) {
      return i;
    }
  }
  fprintf(stderr, "%s error: no more group categories are allowed\n",
          __func__);
  return static_cast<size_t>(-1);
}

}  // anonymous namespace

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(PercettoDataSource);

extern "C"
int percetto_init(size_t category_count,
                  struct percetto_category** categories,
                  enum percetto_clock clock_id) {
  struct percetto_init_args args = PERCETTO_INIT_ARGS_DEFAULTS();
  return percetto_init_with_args(category_count, categories, clock_id, &args);
}

extern "C"
int percetto_init_with_args(size_t category_count,
                            struct percetto_category** categories,
                            enum percetto_clock clock_id,
                            const struct percetto_init_args* args) {
  std::lock_guard<std::mutex>(s_percetto.lock);

  bool is_first_init = !s_percetto.is_initialized;

  if (!s_percetto.is_initialized) {
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
    s_percetto.trace_clock_id = system_clock;
    s_percetto.perfetto_clock = perfetto_clock;
    s_percetto.process_pid = perfetto::base::GetProcessId();

    // Determine system-wide UUID process, as PID is only unique within a
    // namespace.
    s_percetto.process_uuid = GetProcessUuid();

    perfetto::TracingInitArgs init_args;
    init_args.backends = perfetto::kSystemBackend;
    init_args.shmem_size_hint_kb = GetEnvU32(
        "PERCETTO_SHMEM_SIZE_HINT_KB", args->shmem_size_hint_kb);
    init_args.shmem_page_size_hint_kb = GetEnvU32(
        "PERCETTO_SHMEM_PAGE_SIZE_HINT_KB", args->shmem_page_size_hint_kb);
    init_args.shmem_batch_commits_duration_ms = GetEnvU32(
        "PERCETTO_SHMEM_BATCH_COMMITS_DURATION_MS",
        args->shmem_batch_commits_duration_ms);
    perfetto::Tracing::Initialize(init_args);
  }

  size_t in_i = 0;
  size_t store_i = s_percetto.category_count.load(std::memory_order_acquire);
  // Add regular categories.
  for (; in_i < category_count; ++in_i) {
    // Break if and when the categories become group categories.
    if (IsGroupCategory(categories[in_i]))
      break;
    if (store_i == PERCETTO_MAX_CATEGORIES) {
      fprintf(stderr, "%s error: too many categories\n", __func__);
      continue;
    }
    categories[in_i]->name_iid = static_cast<uint64_t>(store_i + 1);
    s_percetto.categories[store_i++] = categories[in_i];
  }
  s_percetto.category_count.store(store_i, std::memory_order_release);

  // Add group categories to the group array.
  for (; in_i < category_count; ++in_i) {
    add_group_category(categories[in_i]);
  }

  return PercettoDataSource::Register(&s_percetto.categories[0],
      &s_percetto.categories[store_i], is_first_init) ? 0 : -1;
}

extern "C"
int percetto_register_group_category(struct percetto_category* category) {
  size_t added = add_group_category(category);
  if (added < PERCETTO_MAX_GROUP_CATEGORIES) {
    // Update the trace enabled state for this aggregate category.
    PercettoDataSource::UpdateGroupCategory(added);
    return 0;
  }

  return -1;
}

extern "C"
int percetto_register_track(struct percetto_track* track) {
  // Lock-free add to array.
  track->parent_uuid.store(s_percetto.process_uuid, std::memory_order_relaxed);
  for (size_t i = 0; i < s_percetto.tracks.max_size(); ++i) {
    struct percetto_track* null_track = NULL;
    // Setup the track data for this slot first so that the data is ready
    // when the track is stored in the array below.
    uint64_t track_uuid = GetTrackUuid(kCustomTrackIdOffset + i);
    track->uuid.store(track_uuid, std::memory_order_relaxed);
    // Try to swap new track into this slot.
    if (s_percetto.tracks[i].compare_exchange_strong(null_track, track)) {
      // Could update incremental state here to immediately send info about
      // this track. Requires new perfetto API to do that.
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
      name, GetTimestampNs(), NULL, 0, NULL);
}

extern "C"
void percetto_event_end(struct percetto_category* category,
                        uint32_t sessions) {
  PercettoDataSource::TraceTrackEvent(
      category, sessions, TrackEvent::Type::TrackEvent_Type_TYPE_SLICE_END,
      NULL, GetTimestampNs(), NULL, 0, NULL);
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
  PercettoDataSource::TraceTrackEvent(
      category, sessions, perfetto_type, data->name, timestamp,
      data->track, data->extra, NULL);
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
  PercettoDataSource::TraceTrackEvent(
      category, sessions, perfetto_type, data->name, timestamp,
      data->track, data->extra, extended);
}
