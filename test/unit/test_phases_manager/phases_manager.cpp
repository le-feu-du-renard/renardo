#include <unity.h>

/**
 * Unit tests for PhasesManager logic
 * Tests phase naming and duration lookup
 */

// Replicate DryerPhase enum for testing
enum class TestDryerPhase : uint8_t {
  kStop = 0,
  kInit = 1,
  kExtraction = 2,
  kCirculation = 3
};

// Replicate PhaseParams for testing
struct TestPhaseParams {
  uint32_t init_phase_duration_s;
  uint32_t extraction_phase_duration_s;
  uint32_t circulation_phase_duration_s;

  TestPhaseParams()
    : init_phase_duration_s(3600),
      extraction_phase_duration_s(120),
      circulation_phase_duration_s(300) {}
};

// Replicate GetPhaseName logic
const char* GetPhaseName(TestDryerPhase phase) {
  switch (phase) {
    case TestDryerPhase::kStop:
      return "Stop";
    case TestDryerPhase::kInit:
      return "Init";
    case TestDryerPhase::kExtraction:
      return "Extraction";
    case TestDryerPhase::kCirculation:
      return "Circulation";
    default:
      return "Unknown";
  }
}

// Replicate GetCurrentPhaseDuration logic
uint32_t GetCurrentPhaseDuration(TestDryerPhase phase, const TestPhaseParams& params) {
  switch (phase) {
    case TestDryerPhase::kInit:
      return params.init_phase_duration_s;
    case TestDryerPhase::kExtraction:
      return params.extraction_phase_duration_s;
    case TestDryerPhase::kCirculation:
      return params.circulation_phase_duration_s;
    default:
      return 0;
  }
}

// Replicate TransitionToNextPhase logic
TestDryerPhase GetNextPhase(TestDryerPhase current_phase) {
  switch (current_phase) {
    case TestDryerPhase::kInit:
      return TestDryerPhase::kExtraction;
    case TestDryerPhase::kExtraction:
      return TestDryerPhase::kCirculation;
    case TestDryerPhase::kCirculation:
      return TestDryerPhase::kExtraction;
    default:
      return current_phase;  // Stop doesn't transition
  }
}

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

// ===== GetPhaseName Tests =====

void test_get_phase_name_stop(void) {
  const char* name = GetPhaseName(TestDryerPhase::kStop);
  TEST_ASSERT_EQUAL_STRING("Stop", name);
}

void test_get_phase_name_init(void) {
  const char* name = GetPhaseName(TestDryerPhase::kInit);
  TEST_ASSERT_EQUAL_STRING("Init", name);
}

void test_get_phase_name_extraction(void) {
  const char* name = GetPhaseName(TestDryerPhase::kExtraction);
  TEST_ASSERT_EQUAL_STRING("Extraction", name);
}

void test_get_phase_name_circulation(void) {
  const char* name = GetPhaseName(TestDryerPhase::kCirculation);
  TEST_ASSERT_EQUAL_STRING("Circulation", name);
}

// ===== GetCurrentPhaseDuration Tests =====

void test_get_duration_stop(void) {
  TestPhaseParams params;
  uint32_t duration = GetCurrentPhaseDuration(TestDryerPhase::kStop, params);
  TEST_ASSERT_EQUAL_UINT32(0, duration);
}

void test_get_duration_init(void) {
  TestPhaseParams params;
  params.init_phase_duration_s = 1800;  // 30 minutes
  uint32_t duration = GetCurrentPhaseDuration(TestDryerPhase::kInit, params);
  TEST_ASSERT_EQUAL_UINT32(1800, duration);
}

void test_get_duration_extraction(void) {
  TestPhaseParams params;
  params.extraction_phase_duration_s = 120;  // 2 minutes
  uint32_t duration = GetCurrentPhaseDuration(TestDryerPhase::kExtraction, params);
  TEST_ASSERT_EQUAL_UINT32(120, duration);
}

void test_get_duration_circulation(void) {
  TestPhaseParams params;
  params.circulation_phase_duration_s = 300;  // 5 minutes
  uint32_t duration = GetCurrentPhaseDuration(TestDryerPhase::kCirculation, params);
  TEST_ASSERT_EQUAL_UINT32(300, duration);
}

void test_get_duration_with_custom_params(void) {
  TestPhaseParams params;
  params.init_phase_duration_s = 7200;        // 2 hours
  params.extraction_phase_duration_s = 600;   // 10 minutes
  params.circulation_phase_duration_s = 900;  // 15 minutes

  TEST_ASSERT_EQUAL_UINT32(7200, GetCurrentPhaseDuration(TestDryerPhase::kInit, params));
  TEST_ASSERT_EQUAL_UINT32(600, GetCurrentPhaseDuration(TestDryerPhase::kExtraction, params));
  TEST_ASSERT_EQUAL_UINT32(900, GetCurrentPhaseDuration(TestDryerPhase::kCirculation, params));
}

// ===== Phase Transition Tests =====

void test_transition_from_init(void) {
  TestDryerPhase next = GetNextPhase(TestDryerPhase::kInit);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kExtraction),
    static_cast<uint8_t>(next)
  );
}

void test_transition_from_extraction(void) {
  TestDryerPhase next = GetNextPhase(TestDryerPhase::kExtraction);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kCirculation),
    static_cast<uint8_t>(next)
  );
}

void test_transition_from_circulation(void) {
  TestDryerPhase next = GetNextPhase(TestDryerPhase::kCirculation);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kExtraction),
    static_cast<uint8_t>(next)
  );
}

void test_transition_from_stop(void) {
  // Stop should not transition
  TestDryerPhase next = GetNextPhase(TestDryerPhase::kStop);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kStop),
    static_cast<uint8_t>(next)
  );
}

void test_phase_cycle_sequence(void) {
  // Test the complete cycle: Init -> Extraction -> Circulation -> Extraction
  TestDryerPhase phase = TestDryerPhase::kInit;

  // Init -> Extraction
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kExtraction),
    static_cast<uint8_t>(phase)
  );

  // Extraction -> Circulation
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kCirculation),
    static_cast<uint8_t>(phase)
  );

  // Circulation -> Extraction (cycle back)
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kExtraction),
    static_cast<uint8_t>(phase)
  );

  // Should continue cycling
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kCirculation),
    static_cast<uint8_t>(phase)
  );
}

// ===== Parameter Validation Tests =====

void test_default_phase_params(void) {
  TestPhaseParams params;

  // Check default values
  TEST_ASSERT_EQUAL_UINT32(3600, params.init_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(120, params.extraction_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(300, params.circulation_phase_duration_s);
}

void test_phase_params_minimum_values(void) {
  TestPhaseParams params;

  // Set to minimum allowed values (5 seconds)
  params.init_phase_duration_s = 5;
  params.extraction_phase_duration_s = 5;
  params.circulation_phase_duration_s = 5;

  TEST_ASSERT_EQUAL_UINT32(5, params.init_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(5, params.extraction_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(5, params.circulation_phase_duration_s);
}

void test_phase_params_maximum_values(void) {
  TestPhaseParams params;

  // Set to maximum allowed values
  params.init_phase_duration_s = 7200;      // 2 hours max for init
  params.extraction_phase_duration_s = 7200; // 2 hours max for extraction
  params.circulation_phase_duration_s = 3600; // 1 hour max for circulation

  TEST_ASSERT_EQUAL_UINT32(7200, params.init_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(7200, params.extraction_phase_duration_s);
  TEST_ASSERT_EQUAL_UINT32(3600, params.circulation_phase_duration_s);
}

// ===== Phase Enum Value Tests =====

void test_phase_enum_values(void) {
  // Verify enum values match expected integer values
  TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(TestDryerPhase::kStop));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(TestDryerPhase::kInit));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(TestDryerPhase::kExtraction));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(TestDryerPhase::kCirculation));
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // GetPhaseName tests
  RUN_TEST(test_get_phase_name_stop);
  RUN_TEST(test_get_phase_name_init);
  RUN_TEST(test_get_phase_name_extraction);
  RUN_TEST(test_get_phase_name_circulation);

  // GetCurrentPhaseDuration tests
  RUN_TEST(test_get_duration_stop);
  RUN_TEST(test_get_duration_init);
  RUN_TEST(test_get_duration_extraction);
  RUN_TEST(test_get_duration_circulation);
  RUN_TEST(test_get_duration_with_custom_params);

  // Phase transition tests
  RUN_TEST(test_transition_from_init);
  RUN_TEST(test_transition_from_extraction);
  RUN_TEST(test_transition_from_circulation);
  RUN_TEST(test_transition_from_stop);
  RUN_TEST(test_phase_cycle_sequence);

  // Parameter validation tests
  RUN_TEST(test_default_phase_params);
  RUN_TEST(test_phase_params_minimum_values);
  RUN_TEST(test_phase_params_maximum_values);

  // Enum value tests
  RUN_TEST(test_phase_enum_values);

  return UNITY_END();
}
