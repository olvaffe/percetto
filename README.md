# Percetto

Percetto is a minimal C wrapper for Perfetto SDK to enable app-specific
tracing.  Internally, there is a minimal implementation of `TrackEvent` data
source.

Percetto is expected to be copied and modified.  This is not because of
performance concern, but more because of the currently inflexible API.  It
should be possible to come up with an API that is both (subjectively) flexible
and efficient.  Percetto can then be turned into a proper shared library.

## Directory Structure

`perfetto-sdk` contains rules to locate and build Perfetto SDK.

`src` contains the source code of Percetto.  It is expected to be copied and
modified.

`examples` contains examples to demonstrate different ways of using Percetto.
