# PerCetto

PerCetto is a minimal C wrapper for Perfetto SDK to enable app-specific
tracing. Internally, there is a minimal implementation of `TrackEvent` data
source.

## Current Limitations

PerCetto works best when statically linked once for a whole process.
When linked into a shared library, PerCetto symbols should not be exported,
because percetto_init can not yet be called multiple times.

If there are multiple instances of PerCetto in a single process, the main
disadvantages are the additional binary size overhead and additional background
thread for Perfetto. The binary size is typically about 450KB.

## Building PerCetto

Clone a [recent release](https://github.com/google/perfetto/releases) of
Perfetto to get the Perfetto SDK (perfetto.h and perfetto.cc).

Configure and build PerCetto:
```
meson build -Dperfetto-sdk=path/to/perfetto/sdk
meson compile -C build
```

## Directory Structure

`perfetto-sdk` contains rules to locate and build Perfetto SDK.

`src` contains the source code of Percetto.

`examples` contains examples to demonstrate different ways of using Percetto.

## Select Examples

### [multi-category.c](examples/multi-category.c)

Shows various types of trace events and the use of multiple categories.

### [atrace.c](examples/atrace.c)

Shows how to use the Android ATRACE macro compatibility API found in
[percetto-atrace.h](src/percetto-atrace.h).

### [timestamps.c](examples/timestamps.c)

Shows how to manually set timestamps for events. This enables use cases where
the events are added to Percetto after they have already occurred, such as
GPU tracing.
