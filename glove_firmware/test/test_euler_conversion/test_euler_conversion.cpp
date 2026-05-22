/* =============================================================================
 * EdgeAI Data Glove V3 — Euler Conversion & Signal Processing Tests
 * =============================================================================
 * Pure-computation tests that run without hardware.
 * Tests quaternion→Euler conversion, SlidingWindow, FeatureNormalizer.
 * =============================================================================
 */

#include <unity.h>
#include <cstring>
#include <cmath>

// Arduino framework requires setup/loop — stubs for test binary
void setup() {}
void loop() {}

// Include project headers
#include "data_structures.h"
#include "SlidingWindow.h"
#include "FeatureNormalizer.h"

// =============================================================================
// Quaternion → Euler Conversion Tests (pure math, no hardware needed)
// =============================================================================

// Re-implement the conversion here for testing since SensorManager needs BNO085
static void quatToEuler(float qw, float qx, float qy, float qz,
                        float& roll, float& pitch, float& yaw) {
    float sinr_cosp = 2.0f * (qw * qx + qy * qz);
    float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
    roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / M_PI;

    float sinp = 2.0f * (qw * qy - qz * qx);
    if (fabsf(sinp) >= 1.0f) {
        pitch = copysignf(90.0f, sinp);
    } else {
        pitch = asinf(sinp) * 180.0f / M_PI;
    }

    float siny_cosp = 2.0f * (qw * qz + qx * qy);
    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
    yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}

void test_euler_identity(void) {
    float roll, pitch, yaw;
    quatToEuler(1.0f, 0.0f, 0.0f, 0.0f, roll, pitch, yaw);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, roll);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pitch);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, yaw);
}

void test_euler_90_pitch(void) {
    float roll, pitch, yaw;
    quatToEuler(0.7071f, 0.0f, 0.7071f, 0.0f, roll, pitch, yaw);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, roll);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 90.0f, pitch);  // Allow numerical error from 0.7071
}

void test_euler_90_yaw(void) {
    float roll, pitch, yaw;
    quatToEuler(0.7071f, 0.0f, 0.0f, 0.7071f, roll, pitch, yaw);

    TEST_ASSERT_FLOAT_WITHIN(5.0f, 90.0f, yaw);
}

void test_euler_90_roll(void) {
    float roll, pitch, yaw;
    quatToEuler(0.7071f, 0.7071f, 0.0f, 0.0f, roll, pitch, yaw);

    TEST_ASSERT_FLOAT_WITHIN(5.0f, 90.0f, roll);
}

void test_euler_negative_pitch(void) {
    float roll, pitch, yaw;
    quatToEuler(0.7071f, 0.0f, -0.7071f, 0.0f, roll, pitch, yaw);

    TEST_ASSERT_FLOAT_WITHIN(5.0f, -90.0f, pitch);
}

// =============================================================================
// SlidingWindow Tests
// =============================================================================

static SlidingWindow* window = nullptr;

void setUp(void) {
    window = new SlidingWindow();
}

void tearDown(void) {
    delete window;
    window = nullptr;
}

void test_window_initial_state(void) {
    TEST_ASSERT_FALSE(window->isFull());
    TEST_ASSERT_EQUAL(0, window->getFrameCount());
}

void test_window_push_single(void) {
    float features[FEATURE_COUNT];
    memset(features, 0, sizeof(features));

    uint16_t pos = window->push(features);
    TEST_ASSERT_EQUAL(0, pos);
    TEST_ASSERT_EQUAL(1, window->getFrameCount());
    TEST_ASSERT_FALSE(window->isFull());
}

void test_window_fill_complete(void) {
    float features[FEATURE_COUNT];
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        for (uint8_t j = 0; j < FEATURE_COUNT; j++) {
            features[j] = (float)i;
        }
        window->push(features);
    }

    TEST_ASSERT_TRUE(window->isFull());
    TEST_ASSERT_EQUAL(WINDOW_SIZE, window->getFrameCount());
}

void test_window_overwrite_oldest(void) {
    float features[FEATURE_COUNT];

    // Fill with frame index as feature value
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        for (uint8_t j = 0; j < FEATURE_COUNT; j++) {
            features[j] = (float)i;
        }
        window->push(features);
    }

    // Push one more — should overwrite frame 0
    for (uint8_t j = 0; j < FEATURE_COUNT; j++) {
        features[j] = 999.0f;
    }
    window->push(features);

    TEST_ASSERT_TRUE(window->isFull());
    TEST_ASSERT_EQUAL(WINDOW_SIZE, window->getFrameCount());

    // Verify frame 0 was overwritten: buffer[0*FEATURE_COUNT + 0] should be 999.0f
    const float* buf = window->getBuffer();
    // After wrapping, the newest frame is at position 0 (since write_idx wrapped)
    TEST_ASSERT_EQUAL_FLOAT(999.0f, buf[0 * FEATURE_COUNT + 0]);
}

void test_window_clear(void) {
    float features[FEATURE_COUNT];
    memset(features, 0, sizeof(features));

    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        window->push(features);
    }

    window->clear();

    TEST_ASSERT_FALSE(window->isFull());
    TEST_ASSERT_EQUAL(0, window->getFrameCount());
}

// =============================================================================
// FeatureNormalizer Tests
// =============================================================================

void test_normalizer_initial_state(void) {
    FeatureNormalizer norm;
    TEST_ASSERT_FALSE(norm.isCalibrated());
    TEST_ASSERT_EQUAL(0, norm.getSampleCount());
}

void test_normalizer_single_sample(void) {
    FeatureNormalizer norm;
    float features[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        features[i] = 10.0f;
    }

    norm.updateStats(features);
    TEST_ASSERT_TRUE(norm.isCalibrated());
    TEST_ASSERT_EQUAL(1, norm.getSampleCount());

    norm.normalize(features);
    // Single sample: min=max → all features = 0.5
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, features[i]);
    }
}

void test_normalizer_range(void) {
    FeatureNormalizer norm;

    float min_sample[FEATURE_COUNT], max_sample[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        min_sample[i] = 0.0f;
        max_sample[i] = 100.0f;
    }

    norm.updateStats(min_sample);
    norm.updateStats(max_sample);
    TEST_ASSERT_EQUAL(2, norm.getSampleCount());

    // Midpoint should normalize to 0.5
    float mid[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        mid[i] = 50.0f;
    }
    norm.normalize(mid);

    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, mid[i]);
    }

    // Range should be 100 for all channels
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, norm.getRange(i));
    }
}

void test_normalizer_clamping(void) {
    FeatureNormalizer norm;

    float min_s[FEATURE_COUNT], max_s[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        min_s[i] = 0.0f;
        max_s[i] = 100.0f;
    }
    norm.updateStats(min_s);
    norm.updateStats(max_s);

    float test[FEATURE_COUNT];
    test[0] = -50.0f;    // Below min → should clamp to 0.0
    test[1] = 150.0f;    // Above max → should clamp to 1.0
    for (uint8_t i = 2; i < FEATURE_COUNT; i++) {
        test[i] = 50.0f;
    }
    norm.normalize(test);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, test[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, test[1]);
}

void test_normalizer_reset(void) {
    FeatureNormalizer norm;
    float features[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        features[i] = 42.0f;
    }

    norm.updateStats(features);
    TEST_ASSERT_TRUE(norm.isCalibrated());

    norm.reset();
    TEST_ASSERT_FALSE(norm.isCalibrated());
    TEST_ASSERT_EQUAL(0, norm.getSampleCount());
}

int main(void) {
    UNITY_BEGIN();

    // Euler conversion tests (pure math)
    RUN_TEST(test_euler_identity);
    RUN_TEST(test_euler_90_pitch);
    RUN_TEST(test_euler_90_yaw);
    RUN_TEST(test_euler_90_roll);
    RUN_TEST(test_euler_negative_pitch);

    // SlidingWindow tests
    RUN_TEST(test_window_initial_state);
    RUN_TEST(test_window_push_single);
    RUN_TEST(test_window_fill_complete);
    RUN_TEST(test_window_overwrite_oldest);
    RUN_TEST(test_window_clear);

    // FeatureNormalizer tests
    RUN_TEST(test_normalizer_initial_state);
    RUN_TEST(test_normalizer_single_sample);
    RUN_TEST(test_normalizer_range);
    RUN_TEST(test_normalizer_clamping);
    RUN_TEST(test_normalizer_reset);

    UNITY_END();
}