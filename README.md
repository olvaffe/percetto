# Percetto

Percetto is a minimal C wrapper of Perfetto SDK to enable app-specific
tracing.  Internally, there is a minimal implementation of `TrackEvent` data
source.

Percetto is designed to be copied and modified.  This is not because of
performance concern, but because of the inflexibility of the current C API.
It should be possible to come up with an API that is both flexible (enough)
and stable, if there are interests.
