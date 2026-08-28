#ifndef NTP_CLOCK_H
#define NTP_CLOCK_H

#include <Arduino.h>

#include "config.h"

// Wall-clock time, for stamping samples the way InfluxDB used to stamp them
// itself.
//
// OTLP carries a real per-sample Unix-ns timestamp — there is no server-side
// "stamp on arrival" the way there was with InfluxDB — and this board has no
// RTC. arduino-pico's WiFi core already brings in an SNTP
// client (WiFiNTP.h's global `NTP`), so getting the time costs nothing beyond
// calling it: this class only adds the non-blocking "has it landed yet" gate,
// the same shape WifiLink adds around association.
//
// **Nothing here blocks.** `Begin()` starts SNTP and returns immediately;
// `Update()` costs one `time(nullptr)` call. Call `Begin()` only once WiFi is
// associated — SNTP needs a route out — and it is safe to call every tick
// after that, since it no-ops once started.
class NtpClock
{
public:
  explicit NtpClock(const char *server);

  // Starts SNTP, once. Safe to call repeatedly — later calls are no-ops.
  void Begin();

  // Notices when the first sync has landed. Cheap enough to call every tick.
  void Update(uint32_t now);

  // True once time(nullptr) has returned a plausible wall-clock value. Nothing
  // may format a sample's timestamp before this is true — see
  // NowUnixMs()'s doc comment.
  bool IsSynced() const { return synced_; }

  // Current wall-clock time in Unix milliseconds. Only meaningful once
  // IsSynced() — the caller's job to check, not this class's, the same
  // division of responsibility WifiLink::GetAddress() leaves to its caller.
  uint64_t NowUnixMs() const;

private:
  const char *server_;
  bool        started_;
  bool        synced_;
};

#endif // NTP_CLOCK_H
