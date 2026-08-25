#ifndef DRYER_METRIC_IDS_H
#define DRYER_METRIC_IDS_H

#include <stdint.h>

// The dryer's own catalog of metric ids for the extension port.
//
// ExtensionProtocol.h's telemetry envelope carries a generic
// {metric_id, value} table and has no idea what any of these ids mean — that
// is the whole point of it being generic. This is where the dryer decides
// what it has to say and gives each reading a number.
//
// A collector on the other end (data-orchestra) resolves these same numbers
// to names through its own include/deployment/metric_catalog.h, a file that
// mirrors this one by number, not by #include — the two must agree on what
// id 0 means, but neither compiles against the other. An id this file adds
// without a matching catalog entry still arrives and still gets stored; it
// just shows up under a numeric fallback name until the catalog is updated.
// Mirror any change here into that file's table.
//
// Ids below 0x8000 only — ExtEncodeValue's tenths encoding and
// ExtEncodeCounter's raw encoding both take care of setting bit 15
// themselves (see ExtensionMetricKind in ExtensionProtocol.h); an id defined
// here should never set it.
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

#endif // DRYER_METRIC_IDS_H
