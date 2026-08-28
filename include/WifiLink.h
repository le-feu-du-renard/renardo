#ifndef WIFI_LINK_H
#define WIFI_LINK_H

#include <Arduino.h>

#include "config.h"

// The Pico W's own radio, associated and kept associated.
//
// **Nothing here blocks. At all.** Not for a scan, not for an association, not
// for a timeout — every call returns in the time it takes to read a status
// register, and an attempt in flight is waited out across successive Update()
// calls rather than inside one.
//
// This is the one place where the dryer's copy of this class differs from
// ../dryer-extension's, which waits out WIFI_CONNECT_TIMEOUT_MS in a
// delay(100) loop. That would be fatal here twice over. The dryer arms an 8 s
// hardware watchdog, and 15 s inside one call is a reboot; and this runs on the
// core that owns both RS485 segments, so the same 15 s would age the inlet
// probe past SENSOR_TIMEOUT_MS and have core 0 cut the heating. An access point
// that is off for the evening must cost this dryer nothing.
//
// Association is attempted on a doubling backoff between WIFI_RETRY_MIN_MS and
// WIFI_RETRY_MAX_MS — the same shape as RemoteModule's BackoffGate, and for the
// same reason: talking to something that is not there should get cheaper the
// longer it has been absent.
class WifiLink
{
public:
  WifiLink(const char *ssid, const char *password);

  // Starts the radio and launches the first association attempt without
  // waiting for it. Always returns false — nothing can have associated yet —
  // and that is not a failure to act on. Update() carries the attempt.
  bool Begin();

  // Advances whatever is in flight: an attempt against its deadline, an
  // association against a drop, an idle backoff against the clock. Costs one
  // status read in the common case. Returns true while associated.
  bool Update(uint32_t now);

  bool IsAssociated() const { return associated_; }

  // The address, for the boot log. An empty string when not associated — a
  // board that logs 0.0.0.0 as though it were an address wastes a bench session
  // for whoever reads it.
  const char *GetAddress() const { return address_; }

  int32_t GetRssi() const;

  // How many times the association has been lost since boot. A link that drops
  // once a week is a different problem from one that drops hourly, and only a
  // counter tells them apart.
  uint32_t GetDropCount() const { return drops_; }

private:
  // Launches an attempt and returns immediately. Update() decides later whether
  // it landed.
  void BeginAttempt(uint32_t now);

  // An attempt that ran out of time: back off, and stop waiting on it.
  void AbandonAttempt(uint32_t now);

  void CaptureAddress();
  void NoteAssociated(const char *how);

  const char *ssid_;
  const char *password_;

  bool associated_;
  char address_[16]; // "255.255.255.255"

  // An attempt is in flight, and attempt_deadline_ms_ is when to give up on it.
  bool     connecting_;
  uint32_t attempt_deadline_ms_;

  uint32_t next_attempt_ms_;
  uint32_t retry_interval_ms_;
  bool     attempted_;

  uint32_t drops_;
};

#endif // WIFI_LINK_H
