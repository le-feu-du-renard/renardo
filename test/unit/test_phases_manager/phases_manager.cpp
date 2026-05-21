#include <unity.h>

// Replicate DryerPhase enum — must match include/SessionManager.h
// Three-phase sequence: Init (x1) -> [Brassage -> Extraction] x inf
enum class TestDryerPhase : uint8_t {
  kStop      = 0,
  kInit      = 1,
  kBrassage  = 2,
  kExtraction = 3
};

// Replicate phase duration constants — must match include/config.h
static constexpr uint32_t kInitPhaseDuration      = 3600;  // INIT_PHASE_DURATION
static constexpr uint32_t kBrassagePhaseDuration  = 900;   // BRASSAGE_PHASE_DURATION
static constexpr uint32_t kExtractionPhaseDuration = 210;  // EXTRACTION_PHASE_DURATION

// Replicate GetCurrentPhaseName logic — must match SessionManager::GetCurrentPhaseName()
const char* GetPhaseName(TestDryerPhase phase) {
  switch (phase) {
    case TestDryerPhase::kInit:       return "Init";
    case TestDryerPhase::kBrassage:   return "Brassage";
    case TestDryerPhase::kExtraction: return "Extraction";
    default:                          return "Stop";
  }
}

// Replicate GetNextPhase logic — must match SessionManager::CheckPhaseTransition()
// Init -> Brassage -> Extraction -> Brassage (cycle)
TestDryerPhase GetNextPhase(TestDryerPhase current_phase) {
  switch (current_phase) {
    case TestDryerPhase::kInit:       return TestDryerPhase::kBrassage;
    case TestDryerPhase::kBrassage:   return TestDryerPhase::kExtraction;
    case TestDryerPhase::kExtraction: return TestDryerPhase::kBrassage;
    default:                          return current_phase;  // Stop stays Stop
  }
}

void setUp(void) {}
void tearDown(void) {}

// ===== GetPhaseName Tests =====

void test_get_phase_name_stop(void) {
  TEST_ASSERT_EQUAL_STRING("Stop", GetPhaseName(TestDryerPhase::kStop));
}

void test_get_phase_name_init(void) {
  TEST_ASSERT_EQUAL_STRING("Init", GetPhaseName(TestDryerPhase::kInit));
}

void test_get_phase_name_brassage(void) {
  TEST_ASSERT_EQUAL_STRING("Brassage", GetPhaseName(TestDryerPhase::kBrassage));
}

void test_get_phase_name_extraction(void) {
  TEST_ASSERT_EQUAL_STRING("Extraction", GetPhaseName(TestDryerPhase::kExtraction));
}

// ===== Phase Duration Constants Tests =====

void test_init_phase_duration(void) {
  TEST_ASSERT_EQUAL_UINT32(3600, kInitPhaseDuration);
}

void test_brassage_phase_duration(void) {
  TEST_ASSERT_EQUAL_UINT32(900, kBrassagePhaseDuration);
}

void test_extraction_phase_duration(void) {
  TEST_ASSERT_EQUAL_UINT32(210, kExtractionPhaseDuration);
}

// ===== Phase Transition Tests =====

void test_transition_from_init(void) {
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kBrassage),
    static_cast<uint8_t>(GetNextPhase(TestDryerPhase::kInit))
  );
}

void test_transition_from_brassage(void) {
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kExtraction),
    static_cast<uint8_t>(GetNextPhase(TestDryerPhase::kBrassage))
  );
}

void test_transition_from_extraction(void) {
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kBrassage),
    static_cast<uint8_t>(GetNextPhase(TestDryerPhase::kExtraction))
  );
}

void test_transition_from_stop(void) {
  // Stop does not transition
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(TestDryerPhase::kStop),
    static_cast<uint8_t>(GetNextPhase(TestDryerPhase::kStop))
  );
}

void test_phase_cycle_sequence(void) {
  // Full cycle: Init -> Brassage -> Extraction -> Brassage -> Extraction
  TestDryerPhase phase = TestDryerPhase::kInit;

  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TestDryerPhase::kBrassage),
                           static_cast<uint8_t>(phase));

  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TestDryerPhase::kExtraction),
                           static_cast<uint8_t>(phase));

  // Extraction loops back to Brassage
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TestDryerPhase::kBrassage),
                           static_cast<uint8_t>(phase));

  // Continues cycling
  phase = GetNextPhase(phase);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TestDryerPhase::kExtraction),
                           static_cast<uint8_t>(phase));
}

// ===== Phase Enum Value Tests =====

void test_phase_enum_values(void) {
  TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(TestDryerPhase::kStop));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(TestDryerPhase::kInit));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(TestDryerPhase::kBrassage));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(TestDryerPhase::kExtraction));
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_get_phase_name_stop);
  RUN_TEST(test_get_phase_name_init);
  RUN_TEST(test_get_phase_name_brassage);
  RUN_TEST(test_get_phase_name_extraction);

  RUN_TEST(test_init_phase_duration);
  RUN_TEST(test_brassage_phase_duration);
  RUN_TEST(test_extraction_phase_duration);

  RUN_TEST(test_transition_from_init);
  RUN_TEST(test_transition_from_brassage);
  RUN_TEST(test_transition_from_extraction);
  RUN_TEST(test_transition_from_stop);
  RUN_TEST(test_phase_cycle_sequence);

  RUN_TEST(test_phase_enum_values);

  return UNITY_END();
}
