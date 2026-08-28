#ifndef METRIC_CATALOG_H
#define METRIC_CATALOG_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "DryerMetricIds.h"
#include "config.h"

// What a metric_id is called, resolved against the compiled catalog in
// DryerMetricIds.h. No RAM table, no state: an id the catalog has no entry for
// still resolves, to a numeric fallback, so a reading is never dropped for want
// of a name.
//
// **One catalog, because the dryer has exactly one producer: itself.**
// ../dryer-extension's copy of this file carries two and chooses between them
// on {device_id, source}, because it relays for a LoRa node as well and two
// producers each numbering from 0 would collide. Nothing reaches this uplink
// that the dryer did not measure, so there is nothing to collide with and no
// choice to make.
//
// Stateless and hardware-free, so `pio test -e native` exercises the real
// thing.
class MetricCatalog
{
public:
  // Writes what `id` is called into `out` (capacity `out_size`), or a numeric
  // fallback when the catalog has no entry — never dropping a reading for want
  // of a name.
  void Name(uint16_t id, char *out, size_t out_size) const
  {
    if (out == nullptr || out_size == 0)
    {
      return;
    }

    for (size_t i = 0; i < kDryerMetricCatalogCount; i++)
    {
      if (kDryerMetricCatalog[i].id == id)
      {
        snprintf(out, out_size, "%s", kDryerMetricCatalog[i].name);
        return;
      }
    }

    snprintf(out, out_size, "metric_%u", id);
  }
};

#endif // METRIC_CATALOG_H
