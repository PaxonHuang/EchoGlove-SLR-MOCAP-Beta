/* =============================================================================
 * EchoGlove V6 — ADC Calibration Persistence Tests
 * =============================================================================
 * Additional tests for NVS calibration path (D-ADC-3).
 * Native tests use stubs; hardware validation requires on-device run.
 *
 * pio test -e native -f test_adc_calibration
 * =============================================================================
 */

#include <unity.h>
#include "Sensors/InternalADCManager.h"

static InternalADCManager adc;

void setUp() {}
void tearDown() {}

void test_hasCalibration_default_false(void) {
    InternalADCManager fresh;
    fresh.begin();
    TEST_ASSERT_FALSE(fresh.hasCalibration());
}

void test_persistCalibration_returns_true(void) {
    adc.begin();
    TEST_ASSERT_TRUE(adc.persistCalibration());
    TEST_ASSERT_TRUE(adc.hasCalibration());
}

void test_setCalibration_bounds_for_test(void) {
    adc.begin();
    uint16_t mn[5] = {100, 200, 300, 400, 500};
    uint16_t mx[5] = {3000, 3100, 3200, 3300, 3400};
    adc.setCalibrationForTest(mn, mx);
    TEST_ASSERT_TRUE(adc.hasCalibration());

    uint16_t outMin[5], outMax[5];
    adc.getCalibrationBounds(outMin, outMax);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(mn[i], outMin[i]);
        TEST_ASSERT_EQUAL_UINT16(mx[i], outMax[i]);
    }
}

void test_getCalibrationBounds_default_12bit(void) {
    InternalADCManager fresh;
    fresh.begin();
    uint16_t mn[5], mx[5];
    fresh.getCalibrationBounds(mn, mx);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(0, mn[i]);
        TEST_ASSERT_EQUAL_UINT16(4095, mx[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hasCalibration_default_false);
    RUN_TEST(test_persistCalibration_returns_true);
    RUN_TEST(test_setCalibration_bounds_for_test);
    RUN_TEST(test_getCalibrationBounds_default_12bit);
    return UNITY_END();
}