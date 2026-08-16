// Unit tests for QuadratureDecoder.
//
// The encoder is now the only way to change a setpoint, so a decoder that
// double-counts or drops detents is a usability bug in every menu at once.

#include <unity.h>
#include "QuadratureDecoder.h"

namespace
{

// Both lines rest HIGH. One detent walks the Gray code all the way back to
// rest, in opposite directions:
//   clockwise         11 -> 01 -> 00 -> 10 -> 11
//   counter-clockwise 11 -> 10 -> 00 -> 01 -> 11
// The two sequences are the same states with A and B exchanged, which is
// exactly what turning the shaft the other way does to the phasing.
const bool kClockwiseA[4] = {false, false, true, true};
const bool kClockwiseB[4] = {true, false, false, true};

// Feed one detent in the given direction and return the total reported.
int FeedDetent(QuadratureDecoder &decoder, bool clockwise)
{
  int total = 0;
  for (int i = 0; i < 4; i++)
  {
    bool a = clockwise ? kClockwiseA[i] : kClockwiseB[i];
    bool b = clockwise ? kClockwiseB[i] : kClockwiseA[i];
    total += decoder.Step(a, b);
  }
  return total;
}

// Bring the decoder to the resting position without counting it.
void Settle(QuadratureDecoder &decoder)
{
  decoder.Step(true, true);
  decoder.Reset();
  decoder.Step(true, true);
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_one_clockwise_detent_reports_one_step(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  TEST_ASSERT_EQUAL_INT(1, FeedDetent(decoder, true));
}

void test_one_counter_clockwise_detent_reports_minus_one(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  TEST_ASSERT_EQUAL_INT(-1, FeedDetent(decoder, false));
}

void test_several_detents_accumulate(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  int total = 0;
  for (int i = 0; i < 5; i++)
  {
    total += FeedDetent(decoder, true);
  }
  TEST_ASSERT_EQUAL_INT(5, total);

  for (int i = 0; i < 3; i++)
  {
    total += FeedDetent(decoder, false);
  }
  TEST_ASSERT_EQUAL_INT(2, total);
}

void test_partial_rotation_reports_nothing(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  // Two of the four transitions: the knob has not reached the next detent.
  int total = 0;
  total += decoder.Step(kClockwiseA[0], kClockwiseB[0]);
  total += decoder.Step(kClockwiseA[1], kClockwiseB[1]);

  TEST_ASSERT_EQUAL_INT(0, total);
}

void test_bouncing_at_a_boundary_produces_no_phantom_detent(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  // The contact rattles between the resting state and the first transition
  // many times without ever completing the cycle.
  int total = 0;
  for (int i = 0; i < 20; i++)
  {
    total += decoder.Step(kClockwiseA[0], kClockwiseB[0]);
    total += decoder.Step(true, true);
  }

  TEST_ASSERT_EQUAL_INT(0, total);
}

void test_reversing_mid_detent_does_not_count(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  int total = 0;
  // Half a detent clockwise...
  total += decoder.Step(kClockwiseA[0], kClockwiseB[0]);
  total += decoder.Step(kClockwiseA[1], kClockwiseB[1]);
  // ...then back the way it came.
  total += decoder.Step(kClockwiseA[0], kClockwiseB[0]);
  total += decoder.Step(true, true);

  TEST_ASSERT_EQUAL_INT(0, total);
}

void test_impossible_transitions_are_dropped(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  // Both lines flipping at once cannot happen on a real encoder; it is noise
  // and must never be scored.
  int total = 0;
  for (int i = 0; i < 10; i++)
  {
    total += decoder.Step(false, false);
    total += decoder.Step(true, true);
  }

  TEST_ASSERT_EQUAL_INT(0, total);
}

void test_reset_clears_partial_progress(void)
{
  QuadratureDecoder decoder;
  Settle(decoder);

  decoder.Step(kClockwiseA[0], kClockwiseB[0]);
  decoder.Step(kClockwiseA[1], kClockwiseB[1]);
  decoder.Reset();

  // The remaining two transitions must not complete the interrupted detent.
  int total = 0;
  total += decoder.Step(kClockwiseA[2], kClockwiseB[2]);
  total += decoder.Step(kClockwiseA[3], kClockwiseB[3]);

  TEST_ASSERT_EQUAL_INT(0, total);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_one_clockwise_detent_reports_one_step);
  RUN_TEST(test_one_counter_clockwise_detent_reports_minus_one);
  RUN_TEST(test_several_detents_accumulate);
  RUN_TEST(test_partial_rotation_reports_nothing);
  RUN_TEST(test_bouncing_at_a_boundary_produces_no_phantom_detent);
  RUN_TEST(test_reversing_mid_detent_does_not_count);
  RUN_TEST(test_impossible_transitions_are_dropped);
  RUN_TEST(test_reset_clears_partial_progress);
  return UNITY_END();
}
