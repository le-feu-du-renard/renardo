#ifndef METRIC_SAMPLES_H
#define METRIC_SAMPLES_H

#include <stddef.h>
#include <stdint.h>

#include "ExtensionProtocol.h"

// Turns one ExtensionTelemetryRecord into OTLP metric samples, one per
// {metric_id, value} tuple it carries.
//
// The dryer is its own producer: the record handed here is the one it has just
// built for the extension port, not one decoded off a wire. So there is no
// device and no source to carry — ../dryer-extension's copy of this file
// multiplexes several producers and needs both, and this one would be
// stamping a constant on every sample.
//
// **OTLP has no multi-field point.** Every reading the wire carries in one
// telemetry block becomes its own metric here — this file has no idea what
// any given metric_id means, it only knows how to turn a
// kExtMetricKindTenths or kExtMetricKindCounter value back into a float and
// hand it onward with its id attached. Naming is deferred entirely to
// whoever builds the OTLP request (see OtlpMetricsBuilder.cpp), which is what
// keeps this file, like the rest of the generic engine, free of any specific
// producer's vocabulary.
//
// **A reading the producer had no value for still produces no sample.**
// kExtMetricKindTenths' sentinel decodes to NAN via ExtDecodeValue, and NAN
// is still the absence test — never emitted as a 0, never emitted at all.
//
// Hardware-free, so `pio test -e native` covers the omissions.

// One metric sample: which metric (by id, resolved to a name only when the
// OTLP request is built), the two attributes every point here carries, a
// value, and the millisecond timestamp it was taken at.
//
// `metric_id` rather than a resolved name string is deliberate: samples sit
// in GrafanaClient's queue for up to GRAFANA_FLUSH_INTERVAL_MS before being
// consumed, and resolving the id against MetricCatalog is cheap enough to
// defer. The id needs no buffer of its own — it is a plain value, copied
// like any other field — and OtlpMetricsBuilder resolves it only at the
// moment it is finally consumed.
struct MetricSample
{
  uint16_t metric_id;
  float    value;
  uint64_t timestamp_ms;
};

// kExtMaxMetrics tuples plus the one sample uptime_s always produces (see
// CollectMetricSamples). Asserted against GRAFANA_QUEUE_CAPACITY by
// test_metric_samples so the two numbers cannot silently drift apart.
constexpr size_t kMetricSamplesPerRecord = kExtMaxMetrics + 1;

// Fills `out` (capacity `max_samples`) with the samples `record` carries at
// `timestamp_ms`, skipping every absent reading. Returns the number written.
size_t CollectMetricSamples(const ExtensionTelemetryRecord &telemetry,
                             uint64_t timestamp_ms, MetricSample *out,
                             size_t max_samples);

#endif // METRIC_SAMPLES_H
