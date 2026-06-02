/* =============================================================================
 * Unit Tests — FlexManager Calibration
 * =============================================================================
 * Tests the full calibration workflow: open hand -> fist -> normalized output.
 * =============================================================================
 */

#include "unity.h"
#include "FlexManager.h"

FlexManager fm;

void setUp() {}
void tearDown() {}

void test_calibration_open_hand_fist() {
    fm.begin(true);
    fm.startCalibration();
    TEST_ASSERT_TRUE(fm.isCalibrating());

    // Simulate open hand (low values) for 300 frames
    float open_vals[5] = {0.1f, 0.05f, 0.08f, 0.06f, 0.07f};
    fm.setSimulatedValues(open_vals);
    for (int i = 0; i < 300; i++) fm.read(nullptr);

    // Simulate fist (high values) for 300 more frames
    float fist_vals[5] = {0.9f, 0.85f, 0.88f, 0.86f, 0.87f};
    fm.setSimulatedValues(fist_vals);
    for (int i = 0; i < 300; i++) fm.read(nullptr);

    TEST_ASSERT_TRUE(fm.isCalibrated());
    TEST_ASSERT_FALSE(fm.isCalibrating());

    // After calibration, open hand should read ~0
    fm.setSimulatedValues(open_vals);
    float out[5];
    fm.read(out);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, out[i]);

    // Fist should read ~1
    fm.setSimulatedValues(fist_vals);
    fm.read(out);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, out[i]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_calibration_open_hand_fist);
    return UNITY_END();
}
