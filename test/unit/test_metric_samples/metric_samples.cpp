// Sample collection: turning one ExtensionTelemetryRecord's generic {id, value}
// tuples into OTLP samples.
//
// The record is the one the dryer has just built for the extension port: it is
// its own producer, so there is no device and no source on a sample to check.
//
// Most of what is asserted here is about what the collector *does not*
// produce. A metric absent from the samples is the honest record of a
// reading the producer had none for; the same metric written as 0 is a lie
// that looks exactly like a measurement, and it is a lie a dashboard cannot
// detect.

#include <unity.h>

#include "ExtensionProtocol.h"
#include "MetricSamples.h"
#include "config.h"

namespace
{

constexpr uint16_t kMetricInlet    = 0;
constexpr uint16_t kMetricWater    = 1;
constexpr uint16_t kMetricTank     = 2; // left absent in the base record
constexpr uint16_t kMetricPosition = 3;
constexpr uint16_t kMetricRunning  = 4;
constexpr uint16_t kMetricCounter  = 5;

ExtensionTelemetryRecord SampleRecord()
{
  ExtensionTelemetryRecord telemetry;

  telemetry.uptime_s = 7200;

  ExtPutMetricValue(telemetry, kMetricInlet, 23.4f);
  ExtPutMetricValue(telemetry, kMetricWater, -0.5f);
  ExtPutMetricValue(telemetry, kMetricTank, NAN);
  ExtPutMetricValue(telemetry, kMetricPosition, 72.0f);
  ExtPutMetricValue(telemetry, kMetricRunning, 1.0f);
  ExtPutMetricCounter(telemetry, kMetricCounter, 3600);

  return telemetry;
}

const MetricSample *Find(const MetricSample *samples, size_t count, uint16_t metric_id)
{
  for (size_t i = 0; i < count; i++)
  {
    if (samples[i].metric_id == metric_id)
    {
      return &samples[i];
    }
  }
  return nullptr;
}

} // namespace

void test_present_readings_produce_samples(void)
{
  const ExtensionTelemetryRecord record = SampleRecord();

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 1700000000000ULL, samples, kMetricSamplesPerRecord);

  TEST_ASSERT_TRUE(count > 0);

  const MetricSample *inlet = Find(samples, count, kMetricInlet);
  TEST_ASSERT_NOT_NULL(inlet);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 23.4f, inlet->value);
  TEST_ASSERT_EQUAL_UINT64(1700000000000ULL, inlet->timestamp_ms);
}

// The point of the whole file.
void test_absent_readings_produce_no_sample(void)
{
  const ExtensionTelemetryRecord record = SampleRecord();

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  TEST_ASSERT_NULL(Find(samples, count, kMetricTank));
}

// A real zero is a measurement and must survive. isnan() is the only test
// that tells the two cases apart.
void test_a_real_zero_is_written(void)
{
  ExtensionTelemetryRecord record = SampleRecord();
  // Overwrite the absent tank reading with a real zero.
  for (uint8_t i = 0; i < record.metric_count; i++)
  {
    if ((record.metrics[i].metric_id & kExtMetricIdMask) == kMetricTank)
    {
      record.metrics[i].value = static_cast<uint16_t>(ExtEncodeValue(0.0f));
    }
  }

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  const MetricSample *tank = Find(samples, count, kMetricTank);
  TEST_ASSERT_NOT_NULL(tank);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, tank->value);
}

// The float passes through the tenths encoding and back — a small rounding
// tolerance is expected, but the sign must never be lost.
void test_negative_values_pass_through(void)
{
  const ExtensionTelemetryRecord record = SampleRecord();

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  const MetricSample *water = Find(samples, count, kMetricWater);
  TEST_ASSERT_NOT_NULL(water);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -0.5f, water->value);
}

void test_positions_pass_through_as_percent(void)
{
  const ExtensionTelemetryRecord record = SampleRecord();

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  const MetricSample *position = Find(samples, count, kMetricPosition);
  TEST_ASSERT_NOT_NULL(position);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 72.0f, position->value);
}

// A raw counter has no sentinel and no absence — it always produces a sample.
void test_counter_metrics_always_present(void)
{
  ExtensionTelemetryRecord record;
  ExtPutMetricCounter(record, kMetricCounter, 0);

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  const MetricSample *counter = Find(samples, count, kMetricCounter);
  TEST_ASSERT_NOT_NULL(counter);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, counter->value);
}

// Uptime is part of the envelope header, not one of the producer's own
// metrics, so it is the one sample a record with no metrics at all still
// produces.
void test_empty_record_still_produces_uptime(void)
{
  ExtensionTelemetryRecord record;
  record.uptime_s = 42;
  // No metrics put at all.

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  TEST_ASSERT_EQUAL_UINT32(1, count);
  TEST_ASSERT_EQUAL_UINT16(kExtMetricUptimeS, samples[0].metric_id);
  TEST_ASSERT_EQUAL_FLOAT(42.0f, samples[0].value);
}

void test_a_full_reading_set_uses_every_slot(void)
{
  ExtensionTelemetryRecord record;

  for (uint16_t i = 0; i < kExtMaxMetrics; i++)
  {
    ExtPutMetricValue(record, i, -123.4f);
  }

  MetricSample samples[kMetricSamplesPerRecord];
  const size_t count = CollectMetricSamples(record, 0, samples, kMetricSamplesPerRecord);

  TEST_ASSERT_EQUAL_UINT32(kMetricSamplesPerRecord, count);
}

// A buffer too small to hold everything stops taking samples rather than
// writing past the caller's array.
void test_short_buffer_stops_cleanly(void)
{
  const ExtensionTelemetryRecord record = SampleRecord();

  MetricSample samples[3];
  const size_t count = CollectMetricSamples(record, 0, samples, 3);

  TEST_ASSERT_EQUAL_UINT32(3, count);
}

// GRAFANA_QUEUE_CAPACITY must hold several full SAMPLE_INTERVAL_MS ticks, so
// the two numbers cannot silently drift apart. One producer here rather than
// ../dryer-extension's four, so the queue holds five ticks rather than one —
// five minutes of readings ridden out across an unreachable gateway before the
// oldest starts being dropped.
void test_several_ticks_fit_the_queue_capacity(void)
{
  TEST_ASSERT_TRUE(kMetricSamplesPerRecord * 5 <= GRAFANA_QUEUE_CAPACITY);
}

void setUp(void)
{
  TestSetMillis(0);
}

void tearDown(void)
{
}

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_present_readings_produce_samples);
  RUN_TEST(test_absent_readings_produce_no_sample);
  RUN_TEST(test_a_real_zero_is_written);
  RUN_TEST(test_negative_values_pass_through);
  RUN_TEST(test_positions_pass_through_as_percent);
  RUN_TEST(test_counter_metrics_always_present);
  RUN_TEST(test_empty_record_still_produces_uptime);
  RUN_TEST(test_a_full_reading_set_uses_every_slot);
  RUN_TEST(test_short_buffer_stops_cleanly);
  RUN_TEST(test_several_ticks_fit_the_queue_capacity);

  return UNITY_END();
}
