// The compiled lookup a metric_id resolves against — one of two catalogs
// this board mirrors, DryerMetricIds.h from the dryer and
// SemaloMetricIds.h from the paired LoRa producer, chosen by {device_id,
// source} rather than searched blindly, so the same id number meaning two
// different things in the two catalogs can never become a naming bug.
//
// What matters here is that an id outside the compiled catalog still
// produces something usable (a numeric fallback) rather than dropping the
// reading — the same guarantee the old wire-learned table gave, now backed
// by a compile-time table instead of one built up over the wire.

#include <unity.h>

#include <string.h>

#include "MetricCatalog.h"
#include "config.h"

void setUp(void) {}
void tearDown(void) {}

void test_an_id_outside_the_catalog_falls_back_to_a_numeric_name(void)
{
  MetricCatalog catalog;

  char name[32];
  catalog.Name(9999, name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("metric_9999", name);
}

void test_a_dryer_id_resolves_to_its_compiled_name(void)
{
  MetricCatalog catalog;

  char name[32];
  catalog.Name(kMetricInletTemperature, name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("inlet_temp", name);
}

void test_several_dryer_ids_are_kept_apart(void)
{
  MetricCatalog catalog;

  char a[32];
  char b[32];
  catalog.Name(kMetricInletTemperature, a, sizeof(a));
  catalog.Name(kMetricInletHumidity, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("inlet_temp", a);
  TEST_ASSERT_EQUAL_STRING("inlet_humidity", b);
}

// The generic engine's own reserved id, not one of the dryer's own metrics —
// still resolves, because it rides along in every producer's catalog.
void test_the_reserved_uptime_id_resolves_for_the_dryer(void)
{
  MetricCatalog catalog;

  char name[32];
  catalog.Name(kExtMetricUptimeS, name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("uptime_s", name);
}

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_an_id_outside_the_catalog_falls_back_to_a_numeric_name);
  RUN_TEST(test_a_dryer_id_resolves_to_its_compiled_name);
  RUN_TEST(test_several_dryer_ids_are_kept_apart);
  RUN_TEST(test_the_reserved_uptime_id_resolves_for_the_dryer);

  return UNITY_END();
}
