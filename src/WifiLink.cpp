#include "config.h"
#if DRYER_WIFI

#include "WifiLink.h"

#include <WiFi.h>

#include "Logger.h"

WifiLink::WifiLink(const char *ssid, const char *password)
    : ssid_(ssid), password_(password), associated_(false), address_{},
      connecting_(false), attempt_deadline_ms_(0), next_attempt_ms_(0),
      retry_interval_ms_(WIFI_RETRY_MIN_MS), attempted_(false), drops_(0)
{
  address_[0] = '\0';
}

bool WifiLink::Begin()
{
  // Station mode explicitly. The core defaults to it, but a board that has once
  // been an access point keeps that setting through a reflash, and the symptom
  // — an SSID appearing in the room instead of a board joining one — is
  // memorable rather than diagnosable.
  WiFi.mode(WIFI_STA);

  BeginAttempt(millis());

  // Nothing can have associated yet: the attempt was launched, not waited on.
  return false;
}

void WifiLink::BeginAttempt(uint32_t now)
{
  attempted_           = true;
  connecting_          = true;
  attempt_deadline_ms_ = now + WIFI_CONNECT_TIMEOUT_MS;

  Logger::Info("WiFi: associating with %s", ssid_);

  // Returns as soon as the request is handed to the CYW43 driver. Whether it
  // worked is Update()'s question, asked one status read at a time.
  WiFi.begin(ssid_, password_);
}

void WifiLink::AbandonAttempt(uint32_t now)
{
  connecting_ = false;

  next_attempt_ms_ = now + retry_interval_ms_;

  retry_interval_ms_ *= 2;
  if (retry_interval_ms_ > WIFI_RETRY_MAX_MS)
  {
    retry_interval_ms_ = WIFI_RETRY_MAX_MS;
  }

  Logger::Warning("WiFi: no association, next attempt in %u s",
                  retry_interval_ms_ / 2000);

  associated_ = false;
  address_[0] = '\0';
}

void WifiLink::NoteAssociated(const char *how)
{
  associated_        = true;
  connecting_        = false;
  retry_interval_ms_ = WIFI_RETRY_MIN_MS;
  CaptureAddress();

  Logger::Info("WiFi: %s, address %s, rssi %d dBm", how, address_, GetRssi());
}

void WifiLink::CaptureAddress()
{
  const IPAddress ip = WiFi.localIP();
  snprintf(address_, sizeof(address_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool WifiLink::Update(uint32_t now)
{
  const bool up = (WiFi.status() == WL_CONNECTED);

  if (up)
  {
    if (!associated_)
    {
      // Either the attempt in flight has just landed, or the link came back on
      // its own without one. The core does reassociate by itself sometimes, and
      // a board that insisted on driving every transition would tear down a
      // link that had already healed.
      NoteAssociated(connecting_ ? "associated" : "reassociated");
    }
    return true;
  }

  if (associated_)
  {
    drops_++;
    associated_ = false;
    address_[0] = '\0';
    Logger::Warning("WiFi: association lost (%u since boot)", drops_);

    // Retry promptly the first time. A drop is usually an access point
    // rebooting, and waiting out a long backoff for that would be waiting for
    // nothing.
    next_attempt_ms_   = now;
    retry_interval_ms_ = WIFI_RETRY_MIN_MS;
  }

  // An attempt in flight owns the radio until its deadline. Calling WiFi.begin()
  // again underneath one is how a board ends up scanning for ever without ever
  // finishing an association.
  if (connecting_)
  {
    if (static_cast<int32_t>(now - attempt_deadline_ms_) >= 0)
    {
      AbandonAttempt(now);
    }
    return false;
  }

  if (!attempted_ || static_cast<int32_t>(now - next_attempt_ms_) >= 0)
  {
    BeginAttempt(now);
  }

  return false;
}

int32_t WifiLink::GetRssi() const
{
  return associated_ ? WiFi.RSSI() : 0;
}

#endif // DRYER_WIFI
