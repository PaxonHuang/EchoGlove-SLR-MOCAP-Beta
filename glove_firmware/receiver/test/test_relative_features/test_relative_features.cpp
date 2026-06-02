#include "unity.h"
#include "RelativeFeatures.h"
#include <cmath>

void setUp() {}
void tearDown() {}

RelativeFeatures rf;

void test_delta_euler() {
    float le[3] = {10, 20, 30}, re[3] = {5, 15, 25};
    float out[6];
    rf.compute(le, nullptr, re, nullptr, out);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[2]);
}

void test_delta_quat_dist_identity() {
    float lq[4] = {1, 0, 0, 0}, rq[4] = {1, 0, 0, 0};
    float out[6];
    rf.compute(nullptr, lq, nullptr, rq, out);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out[3]);
}

void test_delta_gyro_norm() {
    float lg[3] = {1, 2, 3}, rg[3] = {0, 0, 0};
    float out[6];
    rf.compute(nullptr, nullptr, nullptr, nullptr, out, lg, rg);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.74f, out[4]);
}

void test_delta_gyro_axis() {
    float lg[3] = {1, 5, 2}, rg[3] = {1, 1, 2};
    float out[6];
    rf.compute(nullptr, nullptr, nullptr, nullptr, out, lg, rg);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.33f, out[5]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_delta_euler);
    RUN_TEST(test_delta_quat_dist_identity);
    RUN_TEST(test_delta_gyro_norm);
    RUN_TEST(test_delta_gyro_axis);
    return UNITY_END();
}
