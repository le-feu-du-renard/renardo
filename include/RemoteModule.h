#ifndef REMOTE_MODULE_H
#define REMOTE_MODULE_H

#include <Arduino.h>
#include "BackoffGate.h"
#include "Rs485Bus.h"

// Common ground for every slave the dryer talks to on RS485 that is not a bare
// probe: its address, its health, and the rate limiting that keeps an absent one
// from eating the bus.
//
// The hydraulic module and the extension port are opposite relationships — the
// dryer commands the first and reports to the second — but the mechanics are
// identical: one FC16 out, one FC03 back, a timeout deciding availability, and
// an error counter. That part lives here; what the register blocks mean stays
// with the subclass.
//
// Like Rs485Bus, every call blocks and the instance belongs to the core owning
// the bus. There is no locking.
class RemoteModule
{
public:
  // `timeout_ms`: silence after which the module counts as gone.
  // `retry_ms`: how often to poke it once it has been declared absent.
  RemoteModule(Rs485Bus *bus, uint8_t address, const char *name,
               uint32_t timeout_ms, uint32_t retry_ms);

  // True when the module answered within timeout_ms.
  bool IsAvailable() const;

  uint16_t    GetErrorCount() const { return error_count_; }
  uint8_t     GetAddress()    const { return address_; }
  const char *GetName()       const { return name_; }

  // True while failures are being backed off, i.e. the module is being skipped
  // on most cycles.
  bool IsBackedOff() const { return gate_.IsBackedOff(); }

protected:
  // One exchange with the module: write `write_count` registers from
  // `write_reg`, then read `read_count` registers from `read_reg`. Either block
  // may be empty, in which case it is skipped.
  //
  // The read is skipped when the write failed, so a silent module can never cost
  // more than one response timeout per attempt.
  //
  // Returns false both when the exchange failed and when the backoff gate held
  // it back — a skipped attempt is not a failure and increments nothing. Callers
  // that need to tell the two apart should ask IsAvailable().
  bool Exchange(uint16_t write_reg, const uint16_t *write, uint8_t write_count,
                uint16_t read_reg, uint16_t *read, uint8_t read_count);

  Rs485Bus *bus() const { return bus_; }

private:
  Rs485Bus   *bus_;
  uint8_t     address_;
  const char *name_;
  uint32_t    timeout_ms_;

  BackoffGate gate_;

  uint32_t last_success_ms_;
  uint16_t error_count_;
  bool     valid_;

  static constexpr uint16_t kMaxErrors = 100;
};

#endif // REMOTE_MODULE_H
