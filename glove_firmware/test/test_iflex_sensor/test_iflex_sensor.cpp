/* =============================================================================
 * EchoGlove V6 — IFlexSensor Interface Unit Tests
 * =============================================================================
 * Validates the interface contract via a MockFlexSensor implementation.
 * Runs on native platform: pio test -e native -f test_iflex_sensor
 * =============================================================================
 */

#include <unity.h>
#include "Sensors/IFlexSensor.h"

// --- Mock implementation for testing the interface contract ---
class MockFlexSensor : public IFlexSensor {
public:
    bool begin() override {
        _initialized = true;
        return true;
    }

    bool readRaw(uint16_t out[5]) override {
        if (!_initialized) return false;
        for (int i = 0; i < 5; i++) out[i] = _mock_raw[i];
        return true;
    }

    bool readMilliVolts(uint16_t out[5]) override {
        if (!_initialized) return false;
        for (int i = 0; i < 5; i++) out[i] = _mock_mv[i];
        return true;
    }

    bool persistCalibration() override {
        _persisted = true;
        return true;
    }

    bool hasCalibration() const override {
        return _has_calib;
    }

    void setMockRaw(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _mock_raw[i] = vals[i];
    }

    void setMockMV(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _mock_mv[i] = vals[i];
    }

    void setHasCalibration(bool v) { _has_calib = v; }
    bool wasPersisted() const { return _persisted; }

private:
    bool _initialized = false;
    bool _persisted = false;
    bool _has_calib = false;
    uint16_t _mock_raw[5] = {2048, 2048, 2048, 2048, 2048};
    uint16_t _mock_mv[5]  = {1650, 1650, 1650, 1650, 1650};
};

static MockFlexSensor mock;

void setUp() {}
void tearDown() {}

// --- Tests ---

void test_interface_is_polymorphic(void) {
    IFlexSensor* ptr = &mock;
    TEST_ASSERT_NOT_NULL(ptr);
}

void test_begin_returns_true(void) {
    TEST_ASSERT_TRUE(mock.begin());
}

void test_readRaw_returns_5_values(void) {
    mock.begin();
    uint16_t raw[5];
    TEST_ASSERT_TRUE(mock.readRaw(raw));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(raw[i] <= 4095);
    }
}

void test_readRaw_returns_mocked_values(void) {
    mock.begin();
    uint16_t expected[5] = {100, 1000, 2000, 3000, 4000};
    mock.setMockRaw(expected);

    uint16_t actual[5];
    TEST_ASSERT_TRUE(mock.readRaw(actual));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(expected[i], actual[i]);
    }
}

void test_readRaw_fails_before_begin(void) {
    MockFlexSensor m;  // fresh, not begun
    uint16_t raw[5];
    TEST_ASSERT_FALSE(m.readRaw(raw));
}

void test_readMilliVolts_returns_values(void) {
    mock.begin();
    uint16_t mv[5];
    TEST_ASSERT_TRUE(mock.readMilliVolts(mv));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(mv[i] <= 3300);
    }
}

void test_persistCalibration_returns_true(void) {
    mock.begin();
    TEST_ASSERT_TRUE(mock.persistCalibration());
    TEST_ASSERT_TRUE(mock.wasPersisted());
}

void test_hasCalibration_default_false(void) {
    MockFlexSensor m;
    m.begin();
    TEST_ASSERT_FALSE(m.hasCalibration());
}

void test_hasCalibration_settable(void) {
    mock.begin();
    mock.setHasCalibration(true);
    TEST_ASSERT_TRUE(mock.hasCalibration());
}

void test_getCalibrationBounds_default_range(void) {
    MockFlexSensor m;
    m.begin();
    uint16_t mn[5], mx[5];
    m.getCalibrationBounds(mn, mx);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(0, mn[i]);
        TEST_ASSERT_EQUAL_UINT16(4095, mx[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_interface_is_polymorphic);
    RUN_TEST(test_begin_returns_true);
    RUN_TEST(test_readRaw_returns_5_values);
    RUN_TEST(test_readRaw_returns_mocked_values);
    RUN_TEST(test_readRaw_fails_before_begin);
    RUN_TEST(test_readMilliVolts_returns_values);
    RUN_TEST(test_persistCalibration_returns_true);
    RUN_TEST(test_hasCalibration_default_false);
    RUN_TEST(test_hasCalibration_settable);
    RUN_TEST(test_getCalibrationBounds_default_range);
    return UNITY_END();
}
