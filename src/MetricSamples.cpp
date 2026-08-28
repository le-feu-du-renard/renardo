#include "config.h"
#if DRYER_WIFI

#include "MetricSamples.h"

#include <math.h>

namespace
{

// A cursor over the output array — every append goes through it so a full
// array stops taking samples cleanly instead of writing past the caller's
// buffer.
struct Collector
{
  MetricSample *out;
  size_t        capacity;
  size_t        count;
  uint64_t      timestamp_ms;

  bool Put(uint16_t metric_id, float value)
  {
    if (count >= capacity)
    {
      return false;
    }
    out[count++] = MetricSample{metric_id, value, timestamp_ms};
    return true;
  }
};

} // namespace

size_t CollectMetricSamples(const ExtensionTelemetryRecord &telemetry,
                             uint64_t timestamp_ms, MetricSample *out,
                             size_t max_samples)
{
  if (out == nullptr || max_samples == 0)
  {
    return 0;
  }

  Collector c{out, max_samples, 0, timestamp_ms};

  const ExtensionTelemetryRecord &t = telemetry;

  // Uptime always arrives — it is part of the envelope header, not one of the
  // producer's own metrics — so unlike a reading it never has an absence to
  // skip.
  c.Put(kExtMetricUptimeS, static_cast<float>(t.uptime_s));

  for (uint8_t i = 0; i < t.metric_count; i++)
  {
    const ExtensionMetricTuple &tuple = t.metrics[i];
    const uint16_t metric_id = tuple.metric_id & kExtMetricIdMask;

    if ((tuple.metric_id & kExtMetricKindMask) == kExtMetricKindCounter)
    {
      // A raw whole-unit counter has no sentinel and no absence — it always
      // arrives, unlike a reading the producer might not have.
      c.Put(metric_id, static_cast<float>(tuple.value));
      continue;
    }

    const float value = ExtDecodeValue(static_cast<int16_t>(tuple.value));
    if (isnan(value))
    {
      continue; // absent, and deliberately so
    }
    c.Put(metric_id, value);
  }

  return c.count;
}

#endif // DRYER_WIFI
