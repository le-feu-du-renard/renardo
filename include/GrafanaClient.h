#ifndef GRAFANA_CLIENT_H
#define GRAFANA_CLIENT_H

#include <Arduino.h>

#include "MetricSamples.h"
#include "MetricCatalog.h"
#include "config.h"

// Points to Grafana Cloud's OTLP gateway, batched, over HTTPS — replaces
// InfluxClient's line-protocol-over-plain-HTTP write.
//
// **A failing server must not stop this board answering the RS485 master.** Same
// constraint InfluxClient followed: samples accumulate in a fixed queue, the
// queue is flushed on a timer, and when the flush fails the samples stay for
// the next attempt. When the queue is full the *oldest* is dropped, because
// the newest reading is the one worth having.
//
// **The queue holds samples, not formatted lines.** Where InfluxClient's
// queue was one string per InfluxDB point (one record, many fields), this
// one is one MetricSample per field — OTLP has no multi-field point. A
// MetricSample is a fixed-size POD, so unlike InfluxClient's line buffer
// there is no "too long to store" drop: only "queue full", handled the same
// way.
//
// **TLS is not optional here the way it was optional for InfluxClient.**
// InfluxDB sat on the LAN; Grafana Cloud is the public internet and the
// Authorization header travels in every request, so this always speaks
// BearSSL WiFiClientSecure with a pinned CA certificate, never plain
// WiFiClient.
//
// **The Authorization header is a precomputed constant, not built here.**
// Grafana Cloud's OTLP gateway portal hands out a ready-to-use
// `Basic <base64>` value (base64(instance_id:token), the same computation
// Prometheus remote_write needed a hand-rolled encoder for) — `secrets.h`
// carries it whole, so there is no base64 to get wrong on this board.
class GrafanaClient
{
public:
  GrafanaClient(const char *url, const char *authorization, const char *ca_cert);

  void Begin();

  // Queues one sample. Returns false only when the queue was full and the
  // oldest sample had to be dropped to make room for this one.
  bool Enqueue(const MetricSample &sample);

  // Sends everything queued, if the interval has elapsed, there is anything
  // to send, and the preconditions below hold. Returns true when a flush was
  // attempted and succeeded.
  //
  // Neither `network_available` nor `time_synced` being false counts as a
  // server failure — `reachable_` is left alone, the same reasoning
  // InfluxClient::Update applies to a down WiFi link. Reporting the metrics
  // backend as down because NTP has not landed yet would put the wrong fault
  // on the LED.
  //
  // `catalog` resolves each queued sample's metric_id to a name at the
  // moment it is finally sent — not before, and not held onto after, which
  // is why it is a parameter here rather than something this class stores.
  bool Update(uint32_t now, bool network_available, bool time_synced,
              const MetricCatalog &catalog);

  // Sends immediately, regardless of the interval. Returns false when there
  // was nothing to send or the write failed.
  bool Flush(const MetricCatalog &catalog);

  bool IsReachable() const { return reachable_; }
  bool HasAttempted() const { return attempted_; }

  uint16_t GetQueueDepth() const { return depth_; }

  uint32_t GetWrittenCount() const { return written_; }
  uint32_t GetFailedCount() const { return failed_; }
  uint32_t GetDroppedCount() const { return dropped_; }

private:
  void DropOldest();

  const char *url_;
  const char *authorization_;
  const char *ca_cert_;

  // A fixed ring, no heap — same reasoning InfluxClient's line queue gives:
  // a queue that fragments the heap a WiFi stack already lives in fails in a
  // way that looks like a network problem.
  MetricSample queue_[GRAFANA_QUEUE_CAPACITY];
  uint16_t     head_;
  uint16_t     depth_;

  uint32_t last_flush_ms_;
  bool     reachable_;
  bool     attempted_;

  uint32_t written_;
  uint32_t failed_;
  uint32_t dropped_;
};

#endif // GRAFANA_CLIENT_H
