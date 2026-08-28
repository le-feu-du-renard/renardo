#ifndef OTLP_METRICS_BUILDER_H
#define OTLP_METRICS_BUILDER_H

#include <stddef.h>
#include <stdint.h>

#include "MetricSamples.h"
#include "MetricCatalog.h"

// Turns a batch of MetricSample into one Protobuf-encoded OTLP
// ExportMetricsServiceRequest, using the generated structs in include/otlp/.
//
// **One Metric (one Gauge, one NumberDataPoint) per sample, not grouped by
// series.** Same reasoning RemoteWriteBuilder gave for the Prometheus path
// this replaces: this firmware's flush batches different metrics from
// different SAMPLE_INTERVAL_MS ticks together, so grouping would mean
// sorting the queue by (metric, device, source) at flush time. The simpler,
// larger request matches this codebase's existing preference for
// flush-time simplicity over cleverness.
//
// **Every reading is a Gauge**, never a Sum: every telemetry field this
// firmware emits is a point-in-time value, not a running total, including
// the counters (`session_elapsed_s`, `uptime_s`) — a Sum would tell the
// receiver those are safe to reset-and-accumulate across restarts, which
// they are not.
//
// Hardware-free: it only touches nanopb structs and a stack/static buffer, so
// `pio test -e native` exercises the real generated code, not a hand-copied
// simulation of it.

// Encodes `samples[0..count)` as one ExportMetricsServiceRequest into `out`
// (capacity `out_capacity`). Returns the number of bytes written, or 0 when
// the batch does not fit — not truncated, same convention
// InfluxAppendEscaped used.
//
// `catalog` resolves each sample's metric_id to the name a metric is finally
// sent under.
size_t BuildOtlpMetricsRequest(const MetricSample *samples, size_t count, uint8_t *out,
                                size_t out_capacity, const MetricCatalog &catalog);

#endif // OTLP_METRICS_BUILDER_H
