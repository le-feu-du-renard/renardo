#ifndef DRYER_METRIC_IDS_H
#define DRYER_METRIC_IDS_H

#include <stddef.h>
#include <stdint.h>

#include "ExtensionProtocol.h"

// The dryer's own catalog of metric ids and names for the extension port.
//
// ExtensionProtocol.h's telemetry envelope carries a generic
// {metric_id, value} table and has no idea what any of these ids mean — that
// is the whole point of it being generic. This is where the dryer decides
// what it has to say, gives each reading a number, and — via
// kDryerMetricCatalog below — what to call it.
//
// **The collector never compiles any of these names in.** Each telemetry
// block also carries a {id, name} announcement (kExtRegCatalogId /
// kExtRegCatalogName), one entry per cycle, cycling through the whole table
// below; the collector's RuntimeMetricCatalog is the RAM table those
// announcements accumulate into. Renaming or adding a metric here is
// therefore a one-sided edit — the collector picks it up over the wire
// within one lap, no matching file to update on its end.
//
// Names are capped at kExtCatalogNameChars (16) by the wire format, which is
// why these are short forms ("inlet_temp", not "inlet_temperature") rather
// than the fuller names an earlier, per-field wire format used.
//
// Ids below 0x8000 only — ExtPutMetricValue and ExtPutMetricCounter both take
// care of setting bit 15 themselves (see ExtensionMetricKind in
// ExtensionProtocol.h); an id defined here should never set it.
enum DryerMetricId : uint16_t
{
  kMetricInletTemperature  = 0,
  kMetricInletHumidity     = 1,
  kMetricWaterTemperature  = 2,
  kMetricTankTemperature   = 3,
  kMetricTargetTemperature = 4,
  kMetricTargetHumidity    = 5,
  kMetricExtractionPosition = 6,
  kMetricRecyclingPosition = 7,
  kMetricPhase             = 8,

  // Whole seconds, raw counter kind (see ExtPutMetricCounter) — wraps at
  // 65536 s (~18.2 h). A session running longer than that will show the
  // counter roll over; accepted as a documented limitation of the generic
  // envelope rather than a reason to special-case this one field.
  kMetricSessionElapsedS = 9,

  kMetricRunning         = 10,
  kMetricFanOn           = 11,
  kMetricElectricOn      = 12,
  kMetricHydraulicDemand = 13,
  kMetricHydraulicOnline = 14,
  kMetricDamperOpen      = 15,
  kMetricSensorFault     = 16,
  kMetricAirflowFault    = 17,
  kMetricFeedbackFault   = 18,
};

struct DryerMetricCatalogEntry
{
  uint16_t    id;
  const char *name; // kExtCatalogNameChars (16) ASCII characters or fewer
};

// One entry per DryerMetricId above, plus kExtMetricUptimeS — the generic
// engine's own reserved id for ExtensionTelemetryRecord::uptime_s (see
// ExtensionProtocol.h). Uptime is not one of this dryer's own metrics, but
// the collector still needs a name for it, and this catalog is the only
// mechanism that ever reaches the collector, so it rides along here too.
constexpr DryerMetricCatalogEntry kDryerMetricCatalog[] = {
    {kMetricInletTemperature, "inlet_temp"},
    {kMetricInletHumidity, "inlet_humidity"},
    {kMetricWaterTemperature, "water_temp"},
    {kMetricTankTemperature, "tank_temperature"},
    {kMetricTargetTemperature, "target_temp"},
    {kMetricTargetHumidity, "target_humidity"},
    {kMetricExtractionPosition, "extraction_pos"},
    {kMetricRecyclingPosition, "recycling_pos"},
    {kMetricPhase, "phase"},
    {kMetricSessionElapsedS, "session_elapsed"},
    {kMetricRunning, "running"},
    {kMetricFanOn, "fan_on"},
    {kMetricElectricOn, "electric_on"},
    {kMetricHydraulicDemand, "hydraulic_demand"},
    {kMetricHydraulicOnline, "hydraulic_online"},
    {kMetricDamperOpen, "damper_open"},
    {kMetricSensorFault, "sensor_fault"},
    {kMetricAirflowFault, "airflow_fault"},
    {kMetricFeedbackFault, "feedback_fault"},
    {kExtMetricUptimeS, "uptime_s"},
};

constexpr size_t kDryerMetricCatalogCount =
    sizeof(kDryerMetricCatalog) / sizeof(kDryerMetricCatalog[0]);

#endif // DRYER_METRIC_IDS_H
