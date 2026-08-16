#ifndef QUADRATURE_DECODER_H
#define QUADRATURE_DECODER_H

#include <stdint.h>

// Quadrature decoding for a detented rotary encoder. No hardware, no Arduino:
// callers feed it samples of the two lines and it reports completed detents.
//
// An EC11 emits one full Gray-code cycle per detent, so four valid transitions
// make one click of the knob. Running transitions through a table rejects the
// impossible ones — both lines appearing to change at once — which is what
// contact bounce looks like, and the accumulator only releases a detent on a
// complete cycle, so bouncing around a boundary cannot produce phantom steps.
class QuadratureDecoder
{
public:
  QuadratureDecoder() : state_(0), accumulator_(0) {}

  // Feed one sample of the two lines. Returns +1 or -1 on a completed detent,
  // 0 otherwise.
  int8_t Step(bool a, bool b);

  void Reset()
  {
    state_ = 0;
    accumulator_ = 0;
  }

  // Counts per detent for an EC11 (one full quadrature cycle).
  static constexpr int8_t kCountsPerDetent = 4;

private:
  uint8_t state_;
  int8_t  accumulator_;
};

#endif // QUADRATURE_DECODER_H
