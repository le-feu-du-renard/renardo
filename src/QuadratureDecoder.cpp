#include "QuadratureDecoder.h"

// Indexed by (previous state << 2) | current state, both as (A << 1) | B.
// Valid single-line transitions score +/-1; the impossible ones, where both
// lines appear to have changed together, score 0 and are dropped.
static const int8_t kTransitions[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

int8_t QuadratureDecoder::Step(bool a, bool b)
{
  uint8_t current = static_cast<uint8_t>((a ? 2 : 0) | (b ? 1 : 0));
  state_ = static_cast<uint8_t>(((state_ << 2) | current) & 0x0F);

  int8_t movement = kTransitions[state_];
  if (movement == 0)
  {
    return 0;
  }

  accumulator_ = static_cast<int8_t>(accumulator_ + movement);

  if (accumulator_ >= kCountsPerDetent)
  {
    accumulator_ = 0;
    return 1;
  }
  if (accumulator_ <= -kCountsPerDetent)
  {
    accumulator_ = 0;
    return -1;
  }
  return 0;
}
