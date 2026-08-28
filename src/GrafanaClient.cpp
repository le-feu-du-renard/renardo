#include "config.h"
#if DRYER_WIFI

#include "GrafanaClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <stdio.h>
#include <string.h>

#include "Logger.h"
#include "OtlpMetricsBuilder.h"
#include "otlp/metrics_service.pb.h"

namespace
{

// An OTLP/HTTP receiver answers a good export with 200 OK. Grafana Cloud's
// gateway may also answer 202 Accepted while still holding the batch for
// processing — either way the write happened, the same reasoning
// InfluxClient accepted both 200 and 204 for.
bool IsWriteAccepted(int status)
{
  return status == 200 || status == 202;
}

} // namespace

GrafanaClient::GrafanaClient(const char *url, const char *authorization, const char *ca_cert)
    : url_(url), authorization_(authorization), ca_cert_(ca_cert), queue_{}, head_(0), depth_(0),
      last_flush_ms_(0), reachable_(false), attempted_(false), written_(0), failed_(0), dropped_(0)
{
}

void GrafanaClient::Begin()
{
  head_  = 0;
  depth_ = 0;
}

void GrafanaClient::DropOldest()
{
  if (depth_ == 0)
  {
    return;
  }

  head_ = static_cast<uint16_t>((head_ + 1) % GRAFANA_QUEUE_CAPACITY);
  depth_--;
  dropped_++;
}

bool GrafanaClient::Enqueue(const MetricSample &sample)
{
  bool dropped = false;

  if (depth_ >= GRAFANA_QUEUE_CAPACITY)
  {
    DropOldest();
    dropped = true;
  }

  const uint16_t slot = static_cast<uint16_t>((head_ + depth_) % GRAFANA_QUEUE_CAPACITY);
  queue_[slot]         = sample;
  depth_++;

  return !dropped;
}

bool GrafanaClient::Update(uint32_t now, bool network_available, bool time_synced,
                           const MetricCatalog &catalog)
{
  if ((now - last_flush_ms_) < GRAFANA_FLUSH_INTERVAL_MS)
  {
    return false;
  }

  last_flush_ms_ = now;

  if (!network_available || !time_synced)
  {
    return false;
  }

  if (depth_ == 0)
  {
    return false;
  }

  return Flush(catalog);
}

bool GrafanaClient::Flush(const MetricCatalog &catalog)
{
  if (depth_ == 0)
  {
    return false;
  }

  attempted_ = true;

  // Ordered the same way InfluxClient::Flush lays out its own batch: gather
  // what is queued into one contiguous array first, because the ring can
  // wrap and the builder needs a flat MetricSample[].
  static MetricSample batch[GRAFANA_QUEUE_CAPACITY];
  for (uint16_t i = 0; i < depth_; i++)
  {
    const uint16_t slot = static_cast<uint16_t>((head_ + i) % GRAFANA_QUEUE_CAPACITY);
    batch[i]             = queue_[slot];
  }

  static uint8_t protobuf[OTLP_METRICS_SERVICE_PB_H_MAX_SIZE];
  const size_t protobuf_len =
      BuildOtlpMetricsRequest(batch, depth_, protobuf, sizeof(protobuf), catalog);
  if (protobuf_len == 0)
  {
    failed_++;
    Logger::Error("Grafana: %d samples did not fit one export", depth_);
    return false;
  }

  // OTLP/HTTP takes the endpoint's base URL with the signal's path appended —
  // "/v1/metrics" for metrics, per the OpenTelemetry exporter spec.
  char url[192];
  const int url_len = snprintf(url, sizeof(url), "%s/v1/metrics", url_);
  if (url_len < 0 || static_cast<size_t>(url_len) >= sizeof(url))
  {
    failed_++;
    Logger::Error("Grafana: endpoint URL too long");
    return false;
  }

  BearSSL::WiFiClientSecure client;
  client.setCACert(ca_cert_);

  // The hard ceiling that keeps the radio out of the regulation. This runs on
  // core 1, which owns both RS485 segments, and every second spent here is a
  // second the inlet probe is not polled — past SENSOR_TIMEOUT_MS core 0 blocks
  // the heating. Applied to both clients because either can be the one waiting:
  // the TLS handshake before there is an HTTP request, the gateway after.
  //
  // Overrunning is an ordinary failure. The queue is kept, the graph gains a
  // gap, and the next flush tries again — which is already what Flush() does
  // with a refusal.
  client.setTimeout(GRAFANA_FLUSH_BUDGET_MS);

  HTTPClient http;
  http.setTimeout(GRAFANA_FLUSH_BUDGET_MS);
  if (!http.begin(client, url))
  {
    failed_++;
    reachable_ = false;
    Logger::Error("Grafana: could not open %s", url);
    return false;
  }

  http.addHeader("Content-Type", "application/x-protobuf");
  // Precomputed in secrets.h as "Basic <base64(instance_id:token)>" — see
  // GrafanaClient.h's doc comment for why this board never computes it.
  http.addHeader("Authorization", authorization_);

  const int status = http.POST(protobuf, protobuf_len);

  // Read before end(): the body stream closes with the connection, and a
  // status code alone does not say *why* a gateway refused a request, nor
  // prove one actually reached the right stack — a 200 is trusted here, but
  // logging what came back with it is what lets that trust be checked rather
  // than assumed. A negative status is HTTPClient's own error (connection
  // failed, no stream, ...), which never has a body to read.
  const String body = (status > 0) ? http.getString() : String();
  http.end();

  if (!IsWriteAccepted(status))
  {
    failed_++;
    reachable_ = false;

    // A negative status is HTTPClient's own error (connection failed, no
    // stream, read timeout, ...), not an HTTP response — errorToString()
    // names those; for a real HTTP status (a 4xx/5xx from the gateway) it
    // returns nothing and the number alone is the useful part.
    const String reason = HTTPClient::errorToString(status);

    // The queue is left untouched. A server that is restarting should cost a
    // gap in the graph of however long it took, not a hole where the
    // readings were thrown away — same reasoning InfluxClient::Flush gives.
    if (reason.length() > 0)
    {
      Logger::Warning("Grafana: write refused, status %d (%s), %d samples held",
                       status, reason.c_str(), depth_);
    }
    else
    {
      Logger::Warning("Grafana: write refused, status %d, %d samples held", status, depth_);
    }
    if (body.length() > 0)
    {
      // Truncated through a real snprintf: ArduinoLog's own %s has no
      // precision to truncate with, it reads exactly one character after
      // '%' and prints the whole C string handed to it. This line only needs
      // enough of the gateway's error text to say what was wrong, not all of
      // it verbatim.
      char snippet[201];
      snprintf(snippet, sizeof(snippet), "%s", body.c_str());
      Logger::Warning("Grafana: gateway said: %s", snippet);
    }
    return false;
  }

  written_ += depth_;

  const uint16_t sent = depth_;
  head_  = 0;
  depth_ = 0;

  // A 200/202 only means the gateway accepted the bytes — it is not proof the
  // points were indexed and queryable, which is why this line exists rather
  // than trusting the status silently. If the body is non-empty on an
  // accepted write, the gateway had something to say about it despite
  // accepting the request, and that is worth seeing rather than discarding.
  if (body.length() > 0)
  {
    char snippet[151];
    snprintf(snippet, sizeof(snippet), "%s", body.c_str());
    Logger::Info("Grafana: wrote %d sample(s), status %d, gateway said: %s",
                 sent, status, snippet);
  }
  else
  {
    Logger::Info("Grafana: wrote %d sample(s), status %d", sent, status);
  }

  if (!reachable_)
  {
    Logger::Info("Grafana: writing again");
  }
  reachable_ = true;

  return true;
}

#endif // DRYER_WIFI
