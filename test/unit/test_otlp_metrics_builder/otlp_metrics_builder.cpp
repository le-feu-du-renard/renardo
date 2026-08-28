// OtlpMetricsBuilder, round-tripped through nanopb's own pb_decode() against
// the real generated otlp_ExportMetricsServiceRequest_fields descriptor —
// this exercises the actual generated code, not a hand-copied simulation of
// it, the same principle test/unit/support/Arduino.h's own comment states
// about the shim clock.

#include <unity.h>

#include <pb_decode.h>
#include <string.h>

#include "OtlpMetricsBuilder.h"
#include "config.h"
#include "otlp/metrics_service.pb.h"

namespace
{

MetricSample MakeSample(uint16_t metric_id, float value, uint64_t timestamp_ms)
{
  MetricSample sample;
  sample.metric_id    = metric_id;
  sample.value        = value;
  sample.timestamp_ms = timestamp_ms;
  return sample;
}

const char *FindAttribute(const otlp_KeyValue *attributes, pb_size_t count, const char *key)
{
  for (pb_size_t i = 0; i < count; i++)
  {
    if (strcmp(attributes[i].key, key) == 0)
    {
      return attributes[i].value.value.string_value;
    }
  }
  return nullptr;
}

const otlp_Metric *FindMetric(const otlp_ScopeMetrics &scope, const char *name)
{
  for (pb_size_t i = 0; i < scope.metrics_count; i++)
  {
    if (strcmp(scope.metrics[i].name, name) == 0)
    {
      return &scope.metrics[i];
    }
  }
  return nullptr;
}

} // namespace

void test_single_sample_encodes_and_decodes(void)
{
  MetricCatalog catalog;

  const MetricSample samples[] = {
      MakeSample(0, 23.4f, 1700000000000ULL),
  };

  uint8_t      buffer[1024];
  const size_t len = BuildOtlpMetricsRequest(samples, 1, buffer, sizeof(buffer), catalog);
  TEST_ASSERT_TRUE(len > 0);

  otlp_ExportMetricsServiceRequest decoded = otlp_ExportMetricsServiceRequest_init_zero;
  pb_istream_t                     stream  = pb_istream_from_buffer(buffer, len);
  TEST_ASSERT_TRUE(pb_decode(&stream, otlp_ExportMetricsServiceRequest_fields, &decoded));

  TEST_ASSERT_EQUAL_UINT32(1, decoded.resource_metrics_count);
  const otlp_ResourceMetrics &rm = decoded.resource_metrics[0];

  TEST_ASSERT_TRUE(rm.has_resource);
  TEST_ASSERT_EQUAL_STRING(GRAFANA_METRIC_PREFIX,
                            FindAttribute(rm.resource.attributes, rm.resource.attributes_count, "service.name"));

  TEST_ASSERT_EQUAL_UINT32(1, rm.scope_metrics_count);
  const otlp_ScopeMetrics *scope = &rm.scope_metrics[0];

  const otlp_Metric *metric = FindMetric(*scope, "dryer_inlet_temp");
  TEST_ASSERT_NOT_NULL(metric);
  TEST_ASSERT_EQUAL_UINT32(otlp_Metric_gauge_tag, metric->which_data);

  const otlp_Gauge &gauge = metric->data.gauge;
  TEST_ASSERT_EQUAL_UINT32(1, gauge.data_points_count);

  const otlp_NumberDataPoint &dp = gauge.data_points[0];
  TEST_ASSERT_EQUAL_UINT32(otlp_NumberDataPoint_as_double_tag, dp.which_value);
  // Unity's double-precision asserts need UNITY_INCLUDE_DOUBLE, which this
  // project does not define — float precision is enough to catch a wrong
  // value here.
  TEST_ASSERT_EQUAL_FLOAT(23.4f, static_cast<float>(dp.value.as_double));
  TEST_ASSERT_EQUAL_UINT64(1700000000000ULL * 1000000ULL, dp.time_unix_nano);

  // Both attributes are constants, and both are still sent: the same readings
  // can reach Grafana by this radio and by the extension board's relay at once,
  // and two roads must not land in one series claiming to be one measurement
  // taken twice. The relay stamps device="2" source="rs485".
  TEST_ASSERT_EQUAL_STRING(GRAFANA_DEVICE_ID,
                           FindAttribute(dp.attributes, dp.attributes_count, "device"));
  TEST_ASSERT_EQUAL_STRING("wifi",
                           FindAttribute(dp.attributes, dp.attributes_count, "source"));
}

void test_batch_of_samples_produces_one_metric_each(void)
{
  MetricCatalog catalog;

  const MetricSample samples[] = {
      MakeSample(0, 23.4f, 1000),
      MakeSample(10, 1.0f, 1000),
      MakeSample(kExtMetricUptimeS, 42.0f, 1000),
  };

  uint8_t      buffer[2048];
  const size_t len = BuildOtlpMetricsRequest(samples, 3, buffer, sizeof(buffer), catalog);
  TEST_ASSERT_TRUE(len > 0);

  otlp_ExportMetricsServiceRequest decoded = otlp_ExportMetricsServiceRequest_init_zero;
  pb_istream_t                     stream  = pb_istream_from_buffer(buffer, len);
  TEST_ASSERT_TRUE(pb_decode(&stream, otlp_ExportMetricsServiceRequest_fields, &decoded));

  const otlp_ScopeMetrics &scope = decoded.resource_metrics[0].scope_metrics[0];
  TEST_ASSERT_EQUAL_UINT32(3, scope.metrics_count);

  const otlp_Metric *uptime = FindMetric(scope, "dryer_uptime_s");
  TEST_ASSERT_NOT_NULL(uptime);
  const otlp_NumberDataPoint &dp = uptime->data.gauge.data_points[0];
  TEST_ASSERT_EQUAL_STRING(GRAFANA_DEVICE_ID,
                           FindAttribute(dp.attributes, dp.attributes_count, "device"));
}

// A batch that cannot be encoded is not encoded — never truncated, which would
// be a partial export the receiver has no way to recognise as partial.
void test_buffer_too_small_returns_zero(void)
{
  MetricCatalog catalog;

  const MetricSample samples[] = {
      MakeSample(0, 23.4f, 1000),
  };

  uint8_t buffer[4];
  TEST_ASSERT_EQUAL_UINT32(
      0, BuildOtlpMetricsRequest(samples, 1, buffer, sizeof(buffer), catalog));
}

// An id outside the compiled catalog must still produce a metric, under a
// numeric fallback, rather than being dropped.
void test_unknown_id_falls_back_to_a_numeric_name(void)
{
  MetricCatalog catalog;

  const MetricSample samples[] = {
      MakeSample(42, 1.0f, 1000),
  };

  uint8_t      buffer[1024];
  const size_t len = BuildOtlpMetricsRequest(samples, 1, buffer, sizeof(buffer), catalog);
  TEST_ASSERT_TRUE(len > 0);

  otlp_ExportMetricsServiceRequest decoded = otlp_ExportMetricsServiceRequest_init_zero;
  pb_istream_t                     stream  = pb_istream_from_buffer(buffer, len);
  TEST_ASSERT_TRUE(pb_decode(&stream, otlp_ExportMetricsServiceRequest_fields, &decoded));

  const otlp_ScopeMetrics &scope = decoded.resource_metrics[0].scope_metrics[0];
  TEST_ASSERT_NOT_NULL(FindMetric(scope, "dryer_metric_42"));
}

// include/otlp/metrics.options' bound on ScopeMetrics.metrics must equal
// GRAFANA_QUEUE_CAPACITY: a full queue must always fit one export, and a
// mismatch here is a regenerated schema that fell out of sync with
// config.h, not a runtime condition.
void test_generated_capacity_matches_config(void)
{
  otlp_ScopeMetrics scope;
  const size_t       generated_capacity = sizeof(scope.metrics) / sizeof(scope.metrics[0]);
  TEST_ASSERT_EQUAL_UINT32(GRAFANA_QUEUE_CAPACITY, generated_capacity);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_single_sample_encodes_and_decodes);
  RUN_TEST(test_batch_of_samples_produces_one_metric_each);
  RUN_TEST(test_buffer_too_small_returns_zero);
  RUN_TEST(test_unknown_id_falls_back_to_a_numeric_name);
  RUN_TEST(test_generated_capacity_matches_config);

  return UNITY_END();
}
