#include "config.h"
#if DRYER_WIFI

#include "NtpClock.h"

#include <WiFi.h> // pulls in WiFiNTP.h's global `NTP`
#include <time.h>

namespace
{

// The same threshold arduino-pico's own NTPClass::waitSet() uses to decide
// whether time(nullptr) holds a real answer yet: any Unix time below this is
// year-1970-ish, which is what an unsynced clock reads as, never a plausible
// "now". Matching the core's own constant rather than picking a fresh one
// keeps this class's notion of "synced" identical to the one already proven
// in WiFiNTP.h.
constexpr time_t kMinPlausibleUnixTime = 10000000;

} // namespace

NtpClock::NtpClock(const char *server) : server_(server), started_(false), synced_(false) {}

void NtpClock::Begin()
{
  if (started_)
  {
    return;
  }
  started_ = true;

  NTP.begin(server_);
}

void NtpClock::Update(uint32_t now)
{
  (void)now;

  if (!started_ || synced_)
  {
    return;
  }

  if (time(nullptr) >= kMinPlausibleUnixTime)
  {
    synced_ = true;
  }
}

uint64_t NtpClock::NowUnixMs() const
{
  return static_cast<uint64_t>(time(nullptr)) * 1000ULL;
}

#endif // DRYER_WIFI
