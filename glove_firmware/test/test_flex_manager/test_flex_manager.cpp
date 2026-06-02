/* =============================================================================
 * EchoGlove V5 — FlexManager Stub Tests
 * =============================================================================
 * Tests for the FlexManager simulation/calibration stub.
 * Runs on native platform (pio test -e native -f test_flex_manager).
 * =============================================================================
 */

#include <unity.h>
#include "FlexManager.h"

FlexManager fm;

void test_begin() { TEST_ASSERT_TRUE(fm.begin(true)); }

void test_read_returns_5() {
    fm.begin(true);
    float values[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE(fm.read(values));
}

void test_values_normalized() {
    fm.begin(true);
    float v[NUM_FLEX_SENSORS];
    fm.read(v);
    for (int i = 0; i < NUM_FLEX_SENSORS; i++)
        TEST_ASSERT_TRUE(v[i] >= 0.0f && v[i] <= 1.0f);
}

void test_calibration_interface() {
    fm.begin(true);
    fm.startCalibration();
    TEST_ASSERT_TRUE(fm.isCalibrating());
    for (int i = 0; i < 600; i++) fm.read(nullptr);
    TEST_ASSERT_FALSE(fm.isCalibrating());
    TEST_ASSERT_TRUE(fm.isCalibrated());
}

void test_sim_set_values() {
    fm.begin(true);
    float vals[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    fm.setSimulatedValues(vals);
    float out[5];
    fm.read(out);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, vals[i], out[i]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin);
    RUN_TEST(test_read_returns_5);
    RUN_TEST(test_values_normalized);
    RUN_TEST(test_calibration_interface);
    RUN_TEST(test_sim_set_values);
    return UNITY_END();
}
