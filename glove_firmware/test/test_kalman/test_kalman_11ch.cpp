#include "unity.h"
#include "KalmanFilter1D.h"

void setUp() {}
void tearDown() {}

void test_kalman_11_channels() {
    KalmanFilter1D kf(11);
    float input[11] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        10.0f, 20.0f, 30.0f,
                        1.0f, 2.0f, 3.0f};
    float output[11];
    kf.update(input, output);
    // Output should be close to input after warmup
    for (int i = 0; i < 11; i++) {
        TEST_ASSERT_FLOAT_WITHIN(5.0f, input[i], output[i]);
    }
}

void test_kalman_smoothing() {
    KalmanFilter1D kf(11);
    float input[11] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0, 0, 0, 0, 0, 0};
    float output[11];
    // Run several frames to converge
    for (int f = 0; f < 10; f++) kf.update(input, output);
    // Output should be very close to input
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, output[i]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_kalman_11_channels);
    RUN_TEST(test_kalman_smoothing);
    return UNITY_END();
}
