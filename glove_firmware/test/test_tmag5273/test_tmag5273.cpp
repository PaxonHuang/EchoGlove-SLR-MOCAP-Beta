/* =============================================================================
 * EdgeAI Data Glove V3 — TMAG5273 3D Hall Sensor Unit Tests
 * =============================================================================
 * Unity test framework for TMAG5273 driver validation.
 *
 * Run: pio test -e native --filter test_tmag5273
 * =============================================================================
 */

#include <unity.h>
#include <Wire.h>
#include <iostream>

// Arduino framework requires setup/loop — stubs for test binary
void setup() {}
void loop() {}
#include "data_structures.h"
#include "TCA9548A.h"
#include "TMG5273.h"

// Test globals
static TCA9548A* mux = nullptr;
static TMAG5273* sensor = nullptr;

void setUp(void) {
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    mux = new TCA9548A(0x70, &Wire);
    TEST_ASSERT_NOT_NULL(mux);
}

void tearDown(void) {
    delete sensor;
    delete mux;
    sensor = nullptr;
    mux = nullptr;
}

/**
 * @brief Test sensor construction
 */
void test_sensor_construction(void) {
    // Create sensor on mux channel 0
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);

    TEST_ASSERT_EQUAL(0, test_sensor.getMuxChannel());
    TEST_ASSERT_EQUAL(0x22, TMAG5273::DEFAULT_ADDR);
    TEST_ASSERT_EQUAL(40.0f, TMAG5273::FULL_SCALE_MT);
}

/**
 * @brief Test sensor begin() initialization
 */
void test_sensor_begin(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);

    bool success = test_sensor.begin();
    TEST_ASSERT_TRUE(success);
    #endif
}

/**
 * @brief Test device ID verification (should fail with wrong ID)
 */
void test_device_id_verification(void) {
    #ifdef HARDWARE_TEST
    mux->begin();

    // Try to initialize with wrong address (should fail)
    TMAG5273 wrong_sensor(mux, 0, 0x42, &Wire);  // 0x42 is not TMAG5273
    bool success = wrong_sensor.begin();
    TEST_ASSERT_FALSE(success);
    #endif
}

/**
 * @brief Test XYZ reading returns valid values
 */
void test_read_xyz(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);
    test_sensor.begin();

    float x, y, z;
    bool success = test_sensor.readXYZ(&x, &y, &z);

    TEST_ASSERT_TRUE(success);
    // Values should be within ±40mT range
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, z);
    #endif
}

/**
 * @brief Test temperature reading
 */
void test_read_temperature(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);
    test_sensor.begin();

    float temp = test_sensor.readTemperature();
    TEST_ASSERT_FALSE(isnan(temp));

    // Room temperature should be roughly 15-35°C
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 25.0f, temp);
    #endif
}

/**
 * @brief Test trigger conversion (without read)
 */
void test_trigger_conversion(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);
    test_sensor.begin();

    bool success = test_sensor.triggerConversion();
    TEST_ASSERT_TRUE(success);
    #endif
}

/**
 * @brief Test last reading accessors
 */
void test_last_reading_accessors(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    TMAG5273 test_sensor(mux, 0, 0x22, &Wire);
    test_sensor.begin();

    float x, y, z;
    test_sensor.readXYZ(&x, &y, &z);

    TEST_ASSERT_EQUAL_FLOAT(x, test_sensor.getLastX());
    TEST_ASSERT_EQUAL_FLOAT(y, test_sensor.getLastY());
    TEST_ASSERT_EQUAL_FLOAT(z, test_sensor.getLastZ());
    #endif
}

/**
 * @brief Test null pointer handling
 */
void test_null_mux_handling(void) {
    TMAG5273 null_sensor(nullptr, 0, 0x22, &Wire);

    float x, y, z;
    bool success = null_sensor.readXYZ(&x, &y, &z);
    TEST_ASSERT_FALSE(success);  // Should fail gracefully
}

int main(void) {
    UNITY_BEGIN();

    std::cout << "Running TMAG5273 Hall Sensor Tests" << std::endl;
    std::cout << "====================================" << std::endl;

    RUN_TEST(test_sensor_construction);
    RUN_TEST(test_sensor_begin);
    RUN_TEST(test_device_id_verification);
    RUN_TEST(test_read_xyz);
    RUN_TEST(test_read_temperature);
    RUN_TEST(test_trigger_conversion);
    RUN_TEST(test_last_reading_accessors);
    RUN_TEST(test_null_mux_handling);

    UNITY_END();
}
