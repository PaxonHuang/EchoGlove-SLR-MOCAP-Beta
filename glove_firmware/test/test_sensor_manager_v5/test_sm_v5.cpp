/* =============================================================================
 * EchoGlove V5 — SensorManager Simulation Tests
 * =============================================================================
 * Native tests for the V5 simulation-mode SensorManager.
 * Verifies: valid data, range constraints, gesture patterns, feature array
 * layout, and sequence numbering.
 * =============================================================================
 */

#include "unity.h"
#include "SensorManager.h"

SensorManager sm;

void setUp() { sm.begin(true); }

void test_sim_returns_valid_sensor_data() {
    SensorData sd = sm.read();
    TEST_ASSERT_TRUE(sd.seq > 0);
    TEST_ASSERT_TRUE(sd.timestamp_us > 0);
}

void test_sim_flex_in_range() {
    SensorData sd = sm.read();
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(sd.flex[i] >= 0.0f && sd.flex[i] <= 1.0f);
    }
}

void test_sim_euler_in_range() {
    SensorData sd = sm.read();
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(sd.euler[i] >= -180.0f && sd.euler[i] <= 180.0f);
    }
}

void test_sim_gesture_open_hand() {
    sm.setSimulatedGesture(0); // open hand
    SensorData sd = sm.read();
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(sd.flex[i] < 0.3f);
    }
}

void test_sim_gesture_fist() {
    sm.setSimulatedGesture(1); // fist
    SensorData sd = sm.read();
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(sd.flex[i] > 0.7f);
    }
}

void test_sim_to_feature_array() {
    SensorData sd = sm.read();
    float features[SINGLE_HAND_FEATURES];
    sd.toFeatureArray(features);
    TEST_ASSERT_EQUAL_FLOAT(sd.flex[0], features[0]);
    TEST_ASSERT_EQUAL_FLOAT(sd.euler[0], features[5]);
    TEST_ASSERT_EQUAL_FLOAT(sd.gyro[0], features[8]);
}

void test_sim_seq_increments() {
    SensorData sd1 = sm.read();
    SensorData sd2 = sm.read();
    TEST_ASSERT_EQUAL(sd1.seq + 1, sd2.seq);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sim_returns_valid_sensor_data);
    RUN_TEST(test_sim_flex_in_range);
    RUN_TEST(test_sim_euler_in_range);
    RUN_TEST(test_sim_gesture_open_hand);
    RUN_TEST(test_sim_gesture_fist);
    RUN_TEST(test_sim_to_feature_array);
    RUN_TEST(test_sim_seq_increments);
    return UNITY_END();
}
