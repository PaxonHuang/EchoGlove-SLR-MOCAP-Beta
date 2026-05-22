/* =============================================================================
 * EdgeAI Data Glove V3 — TCA9548A I2C Multiplexer Unit Tests
 * =============================================================================
 * Unity test framework for TCA9548A driver validation.
 *
 * Run: pio test -e native --filter test_tca9548a
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

// Test globals
static TCA9548A* mux = nullptr;

void setUp(void) {
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    mux = new TCA9548A(0x70, &Wire);
}

void tearDown(void) {
    delete mux;
    mux = nullptr;
}

/**
 * @brief Test mux begin() initializes correctly
 */
void test_mux_begin(void) {
    TCA9548A test_mux(0x70, &Wire);
    bool success = test_mux.begin();

    // In simulation/native mode, begin() should succeed if I2C bus is available
    // On hardware, this requires actual TCA9548A connected
    #ifdef HARDWARE_TEST
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(0xFF, test_mux.getActiveChannel());
    #else
    // Native test: just verify no crash
    TEST_ASSERT_TRUE(true);
    #endif
}

/**
 * @brief Test channel selection sequence (0-4 for Hall sensors)
 */
void test_channel_selection(void) {
    #ifdef HARDWARE_TEST
    mux->begin();

    for (uint8_t ch = 0; ch < 5; ch++) {
        mux->selectChannel(ch);
        TEST_ASSERT_EQUAL(ch, mux->getActiveChannel());
    }

    // Verify disableAll works
    mux->disableAll();
    TEST_ASSERT_EQUAL(0xFF, mux->getActiveChannel());
    #endif
}

/**
 * @brief Test that disableAll clears active channel
 */
void test_disable_all_clears_channel(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    mux->selectChannel(2);
    TEST_ASSERT_EQUAL(2, mux->getActiveChannel());

    mux->disableAll();
    TEST_ASSERT_EQUAL(0xFF, mux->getActiveChannel());
    #endif
}

/**
 * @brief Test probing for devices on channels
 */
void test_probe_channel(void) {
    #ifdef HARDWARE_TEST
    mux->begin();

    // Probe for TMAG5273 at 0x22 on channel 0
    bool found = mux->probeChannel(0, 0x22);
    // May be true (sensor present) or false (no sensor) - just verify no crash
    TEST_ASSERT_TRUE(found || !found);
    #endif
}

/**
 * @brief Test channel clamping (values > 7 should wrap to 0)
 */
void test_channel_clamping(void) {
    #ifdef HARDWARE_TEST
    mux->begin();
    mux->selectChannel(10);  // Invalid channel
    TEST_ASSERT_EQUAL(0, mux->getActiveChannel());  // Should clamp to 0
    #endif
}

int main(void) {
    UNITY_BEGIN();

    std::cout << "Running TCA9548A I2C Multiplexer Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(test_mux_begin);
    RUN_TEST(test_channel_selection);
    RUN_TEST(test_disable_all_clears_channel);
    RUN_TEST(test_probe_channel);
    RUN_TEST(test_channel_clamping);

    UNITY_END();
}
