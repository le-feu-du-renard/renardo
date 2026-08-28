#include "config.h"
#if DRYER_WIFI

#include "OtlpMetricsBuilder.h"

#include <pb_encode.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "otlp/metrics_service.pb.h"

namespace
{

void SetKeyValue(otlp_KeyValue &kv, const char *key, const char *value)
{
  strncpy(kv.key, key, sizeof(kv.key) - 1);
  kv.key[sizeof(kv.key) - 1] = '\0';

  kv.has_value            = true;
  kv.value.which_value    = otlp_AnyValue_string_value_tag;
  strncpy(kv.value.value.string_value, value, sizeof(kv.value.value.string_value) - 1);
  kv.value.value.string_value[sizeof(kv.value.value.string_value) - 1] = '\0';
}

void FillMetric(otlp_Metric &metric, const MetricSample &sample, const MetricCatalog &catalog)
{
  // Resolved from the id only now, at the moment it is finally consumed —
  // see MetricSamples.h for why the sample itself only ever carries the id.
  // One prefix, because the dryer only ever reports about itself.
  char name[64];
  catalog.Name(sample.metric_id, name, sizeof(name));
  snprintf(metric.name, sizeof(metric.name), "%s_%s", GRAFANA_METRIC_PREFIX, name);

  metric.which_data                = otlp_Metric_gauge_tag;
  metric.data.gauge.data_points_count = 1;

  otlp_NumberDataPoint &dp = metric.data.gauge.data_points[0];

  // OTLP timestamps are Unix nanoseconds; MetricSample carries milliseconds,
  // the resolution NtpClock and the wire's own tenths already agree on.
  dp.time_unix_nano = sample.timestamp_ms * 1000000ULL;

  dp.which_value       = otlp_NumberDataPoint_as_double_tag;
  dp.value.as_double   = static_cast<double>(sample.value);

  // Both attributes are constants here, and both are still sent. The same
  // dryer's readings can arrive in Grafana by two roads at once — straight off
  // this radio, and relayed by the extension board over RS485 — and they must
  // not land in the same series claiming to be one measurement taken twice.
  // The extension board stamps device="2" source="rs485"; this stamps the
  // dryer's own identity and the road it took.
  dp.attributes_count = 2;
  SetKeyValue(dp.attributes[0], "device", GRAFANA_DEVICE_ID);
  SetKeyValue(dp.attributes[1], "source", "wifi");
}

} // namespace

size_t BuildOtlpMetricsRequest(const MetricSample *samples, size_t count, uint8_t *out,
                                size_t out_capacity, const MetricCatalog &catalog)
{
  if (samples == nullptr || out == nullptr || out_capacity == 0)
  {
    return 0;
  }

  // Static rather than a stack local: at GRAFANA_QUEUE_CAPACITY entries this
  // struct runs to several kilobytes, the same reasoning InfluxClient::Flush
  // gives for its own static `body` buffer.
  static otlp_ExportMetricsServiceRequest request;

  otlp_ResourceMetrics &resource_metrics = request.resource_metrics[0];
  request.resource_metrics_count         = 1;

  // One resource identifies every metric in the export as coming from this
  // board's deployment, the OTLP equivalent of the "measurement" concept
  // INFLUX_MEASUREMENT used to carry.
  resource_metrics.has_resource            = true;
  resource_metrics.resource.attributes_count = 1;
  SetKeyValue(resource_metrics.resource.attributes[0], "service.name", GRAFANA_METRIC_PREFIX);

  resource_metrics.scope_metrics_count = 1;
  otlp_ScopeMetrics &scope_metrics     = resource_metrics.scope_metrics[0];

  constexpr size_t kMaxMetrics = sizeof(scope_metrics.metrics) / sizeof(scope_metrics.metrics[0]);
  if (count > kMaxMetrics)
  {
    // Cannot be encoded, so it is not encoded. GrafanaClient::Flush never
    // hands this more than the queue holds, and the queue's capacity is
    // sized to fit — reaching this is a config.h/*.options mismatch, not an
    // expected runtime path.
    return 0;
  }

  scope_metrics.metrics_count = static_cast<pb_size_t>(count);
  for (size_t i = 0; i < count; i++)
  {
    FillMetric(scope_metrics.metrics[i], samples[i], catalog);
  }

  pb_ostream_t stream = pb_ostream_from_buffer(out, out_capacity);
  if (!pb_encode(&stream, otlp_ExportMetricsServiceRequest_fields, &request))
  {
    return 0;
  }

  return stream.bytes_written;
}

#endif // DRYER_WIFI
