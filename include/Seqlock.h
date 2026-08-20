#ifndef SEQLOCK_H
#define SEQLOCK_H

#include <Arduino.h>
#include <hardware/sync.h>

// Single-writer / single-reader seqlock for passing a struct between the cores.
//
// v3 shared four bare volatile floats, so Core 0 could observe a half-updated
// set and had no way to tell a fresh value from one frozen by a dead probe.
// The sequence counter is odd while a write is in flight; a reader that sees an
// odd counter, or a counter that changed across the copy, retries.
//
// The RP2040 cores share SRAM with no data cache, so plain data memory barriers
// are enough to order the counter against the payload.
//
// Templated because three separate crossings now need it — the sensor snapshot
// going up, and the extension port's telemetry and command going the other way.
// Duplicating a memory-barrier protocol per payload is a far worse trade than
// parameterising it.
//
// One core publishes and one core reads. Two writers would corrupt the counter;
// there is no locking here beyond the barriers.
template <typename T>
class Seqlock
{
public:
  Seqlock() : sequence_(0) {}

  // Called by the owning core only.
  void Publish(const T &value)
  {
    sequence_ = sequence_ + 1;  // now odd: write in progress
    __dmb();
    data_ = value;
    __dmb();
    sequence_ = sequence_ + 1;  // now even: write complete
  }

  // Called by the reading core. Blocks only for the duration of a concurrent
  // write, which is a handful of instructions.
  void Read(T &out) const
  {
    while (true)
    {
      uint32_t before = sequence_;
      if (before & 1u)
      {
        continue;  // write in progress, retry
      }
      __dmb();
      out = data_;
      __dmb();
      if (before == sequence_)
      {
        return;  // no write started or finished while we copied
      }
    }
  }

private:
  volatile uint32_t sequence_;
  T                 data_;
};

#endif // SEQLOCK_H
