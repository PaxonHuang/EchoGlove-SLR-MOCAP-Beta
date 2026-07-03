/* =============================================================================
 * EchoGlove V5/V6 — FlexManager Tests (IFlexSensor abstraction)
 * =============================================================================
 * V6: FlexManager now depends on IFlexSensor*. Tests use InternalADCManager
 * (native simulation mode) as the injected flex source.
 *
 * Runs on native: pio test -e native -f test_flex_manager
 * =============================================================================
 */

#include <unity.h>
#include "FlexManager.h"
#include "InternalADCManager.h"

static FlexManager fm;
static InternalADCManager adc;

void setUp() {}
void tearDown() {}

// ---- begin() with IFlexSensor* ----

void test_begin_with_interface(void) {
    TEST_ASSERT_TRUE(fm.begin(&adc));
    TEST_ASSERT_TRUE(fm.isReady());
}

void test_begin_null_sensor_returns_false(void) {
    FlexManager local;
    TEST_ASSERT_FALSE(local.begin(static_cast<IFlexSensor*>(nullptr)));
    TEST_ASSERT_FALSE(local.isReady());
}

void test_begin_simulation_legacy(void) {
    FlexManager local;
    TEST_ASSERT_TRUE(local.begin(true));   // simulation mode
    TEST_ASSERT_FALSE(local.isReady());    // no real sensor
}

// ---- read() ----

void test_read_returns_5_normalized_values(void) {
    fm.begin(&adc);
    adc.begin();
    float values[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE(fm.read(values));
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(values[i] >= 0.0f && values[i] <= 1.0f);
    }
}

void test_read_without_calibration_uses_default_range(void) {
    // With sim raw = 2048, default range 0..4095 → 0.5
    fm.begin(&adc);
    adc.begin();
    uint16_t sim[5] = {2048, 2048, 2048, 2048, 2048};
    adc.setSimRaw(sim);

    float v[NUM_FLEX_SENSORS];
    fm.read(v);
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, v[i]);
    }
}

void test_read_clamps_to_01(void) {
    fm.begin(&adc);
    adc.begin();
    // Sim saturated high → should clamp to 1.0
    uint16_t sim[5] = {4095, 4095, 4095, 4095, 4095};
    adc.setSimRaw(sim);

    float v[NUM_FLEX_SENSORS];
    fm.read(v);
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(v[i] <= 1.0f);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, v[i]);
    }
}

// ---- Calibration ----

void test_calibration_interface(void) {
    fm.begin(&adc);
    adc.begin();
    uint16_t sim[5] = {1000, 1000, 1000, 1000, 1000};
    adc.setSimRaw(sim);

    fm.startCalibration();
    TEST_ASSERT_TRUE(fm.isCalibrating());

    for (int i = 0; i < 600; i++) fm.read(nullptr);
    TEST_ASSERT_FALSE(fm.isCalibrating());
    TEST_ASSERT_TRUE(fm.isCalibrated());
}

void test_calibration_normalizes_correctly(void) {
    fm.begin(&adc);
    adc.begin();
    fm.startCalibration();

    // 300 frames low (min=500), then 300 frames high (max=3500)
    uint16_t low[5]  = {500, 500, 500, 500, 500};
    uint16_t high[5] = {3500, 3500, 3500, 3500, 3500};
    adc.setSimRaw(low);
    for (int i = 0; i < 300; i++) fm.read(nullptr);
    adc.setSimRaw(high);
    for (int i = 0; i < 300; i++) fm.read(nullptr);

    TEST_ASSERT_TRUE(fm.isCalibrated());

    // Mid-range raw = 2000 → (2000-500)/(3500-500) = 0.5
    uint16_t mid[5] = {2000, 2000, 2000, 2000, 2000};
    adc.setSimRaw(mid);
    float v[NUM_FLEX_SENSORS];
    fm.read(v);
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, v[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_with_interface);
    RUN_TEST(test_begin_null_sensor_returns_false);
    RUN_TEST(test_begin_simulation_legacy);
    RUN_TEST(test_read_returns_5_normalized_values);
    RUN_TEST(test_read_without_calibration_uses_default_range);
    RUN_TEST(test_read_clamps_to_01);
    RUN_TEST(test_calibration_interface);
    RUN_TEST(test_calibration_normalizes_correctly);
    return UNITY_END();
}
