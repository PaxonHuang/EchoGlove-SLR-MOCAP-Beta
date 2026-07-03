/* =============================================================================
 * EchoGlove V6 — InternalADCManager Unit Tests
 * =============================================================================
 * Validates the V6 default IFlexSensor implementation.
 * Runs on native platform: pio test -e native -f test_internal_adc
 *
 * Hardware path (analogRead, analogReadMilliVolts, NVS) is guarded by
 * #ifndef UNIT_TEST; native tests exercise the simulation stubs.
 * =============================================================================
 */

#include <unity.h>
#include "Sensors/InternalADCManager.h"

static InternalADCManager adc;

void setUp() {}
void tearDown() {}

// --- begin() / lifecycle ---

void test_begin_returns_true(void) {
    TEST_ASSERT_TRUE(adc.begin());
}

void test_begin_idempotent(void) {
    TEST_ASSERT_TRUE(adc.begin());
    TEST_ASSERT_TRUE(adc.begin());  // second call should not break
}

void test_readRaw_fails_before_begin(void) {
    InternalADCManager fresh;
    uint16_t raw[5];
    TEST_ASSERT_FALSE(fresh.readRaw(raw));
}

void test_readMilliVolts_fails_before_begin(void) {
    InternalADCManager fresh;
    uint16_t mv[5];
    TEST_ASSERT_FALSE(fresh.readMilliVolts(mv));
}

// --- readRaw ---

void test_readRaw_returns_5_channels(void) {
    adc.begin();
    uint16_t raw[5];
    TEST_ASSERT_TRUE(adc.readRaw(raw));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(raw[i] <= 4095);
    }
}

void test_readRaw_returns_mocked_values(void) {
    adc.begin();
    uint16_t expected[5] = {100, 1000, 2000, 3000, 4000};
    adc.setSimRaw(expected);

    uint16_t actual[5];
    TEST_ASSERT_TRUE(adc.readRaw(actual));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(expected[i], actual[i]);
    }
}

void test_readRaw_stable_across_calls(void) {
    // With fixed sim values, consecutive reads should be identical
    adc.begin();
    uint16_t sim[5] = {2048, 2048, 2048, 2048, 2048};
    adc.setSimRaw(sim);

    uint16_t r1[5], r2[5];
    adc.readRaw(r1);
    adc.readRaw(r2);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(r1[i], r2[i]);
    }
}

// --- readMilliVolts ---

void test_readMilliVolts_returns_values(void) {
    adc.begin();
    uint16_t mv[5];
    TEST_ASSERT_TRUE(adc.readMilliVolts(mv));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(mv[i] <= 3300);
    }
}

void test_readMilliVolts_mocked(void) {
    adc.begin();
    uint16_t expected[5] = {900, 1200, 1500, 1800, 2150};
    adc.setSimMV(expected);

    uint16_t actual[5];
    TEST_ASSERT_TRUE(adc.readMilliVolts(actual));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(expected[i], actual[i]);
    }
}

// --- Compile-time constants (spec contract) ---

void test_oversample_is_16(void) {
    TEST_ASSERT_EQUAL(16, InternalADCManager::kOversample);
}

void test_pins_are_gpio1_to_5(void) {
    // 07 spec §3.1: Thumb=GPIO1, Index=2, Middle=3, Ring=4, Pinky=5
    TEST_ASSERT_EQUAL(1, InternalADCManager::kPins[0]);
    TEST_ASSERT_EQUAL(2, InternalADCManager::kPins[1]);
    TEST_ASSERT_EQUAL(3, InternalADCManager::kPins[2]);
    TEST_ASSERT_EQUAL(4, InternalADCManager::kPins[3]);
    TEST_ASSERT_EQUAL(5, InternalADCManager::kPins[4]);
}

void test_default_raw_range_is_12bit(void) {
    TEST_ASSERT_EQUAL_UINT16(0, InternalADCManager::kRawMinDef);
    TEST_ASSERT_EQUAL_UINT16(4095, InternalADCManager::kRawMaxDef);
}

// --- Calibration (NVS path stubbed in native) ---

void test_hasCalibration_default_false(void) {
    InternalADCManager fresh;
    fresh.begin();
    TEST_ASSERT_FALSE(fresh.hasCalibration());
}

void test_persistCalibration_returns_true(void) {
    adc.begin();
    TEST_ASSERT_TRUE(adc.persistCalibration());
}

void test_getCalibrationBounds_default_full_range(void) {
    InternalADCManager fresh;
    fresh.begin();
    uint16_t mn[5], mx[5];
    fresh.getCalibrationBounds(mn, mx);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(0, mn[i]);
        TEST_ASSERT_EQUAL_UINT16(4095, mx[i]);
    }
}

void test_getCalibrationBounds_returns_test_values(void) {
    adc.begin();
    uint16_t mn[5] = {100, 200, 300, 400, 500};
    uint16_t mx[5] = {3000, 3100, 3200, 3300, 3400};
    adc.setCalibrationForTest(mn, mx);

    uint16_t outMin[5], outMax[5];
    adc.getCalibrationBounds(outMin, outMax);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(mn[i], outMin[i]);
        TEST_ASSERT_EQUAL_UINT16(mx[i], outMax[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_returns_true);
    RUN_TEST(test_begin_idempotent);
    RUN_TEST(test_readRaw_fails_before_begin);
    RUN_TEST(test_readMilliVolts_fails_before_begin);
    RUN_TEST(test_readRaw_returns_5_channels);
    RUN_TEST(test_readRaw_returns_mocked_values);
    RUN_TEST(test_readRaw_stable_across_calls);
    RUN_TEST(test_readMilliVolts_returns_values);
    RUN_TEST(test_readMilliVolts_mocked);
    RUN_TEST(test_oversample_is_16);
    RUN_TEST(test_pins_are_gpio1_to_5);
    RUN_TEST(test_default_raw_range_is_12bit);
    RUN_TEST(test_hasCalibration_default_false);
    RUN_TEST(test_persistCalibration_returns_true);
    RUN_TEST(test_getCalibrationBounds_default_full_range);
    RUN_TEST(test_getCalibrationBounds_returns_test_values);
    return UNITY_END();
}
