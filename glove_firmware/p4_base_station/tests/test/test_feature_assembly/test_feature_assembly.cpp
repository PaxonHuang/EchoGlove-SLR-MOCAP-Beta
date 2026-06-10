#include <unity.h>
#include <cstring>
#include <cmath>
#include "data_structures.h"
#include "FramePairer.h"

static GlovePacket make_pkt(uint8_t hand, uint32_t tick,
                             float flex_base, float imu_base) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = hand;
    pkt.tick_id = tick;
    for (int i = 0; i < 5; i++) pkt.flex[i] = flex_base + i * 0.1f;
    for (int i = 0; i < 6; i++) pkt.imu[i] = imu_base + i * 0.2f;
    pkt.computeChecksum();
    return pkt;
}

// Compute relative features from GlovePacket imu[6] (euler[3] + gyro[3])
// NOTE: Cannot use RelativeFeatures.compute() -- it requires quaternion[4]
// which is NOT transmitted in GlovePacket.imu[6].
static void compute_relative_from_imu(
    const float* left_imu, const float* right_imu, float* out) {
    memset(out, 0, 6 * sizeof(float));
    // out[0..2] = delta euler (left - right)
    for (int i = 0; i < 3; i++)
        out[i] = left_imu[i] - right_imu[i];
    // out[3] = euler distance (L2 norm of delta euler)
    float dist_sq = 0;
    for (int i = 0; i < 3; i++) dist_sq += out[i] * out[i];
    out[3] = sqrtf(dist_sq);
    // out[4] = delta gyro norm
    float norm_l = 0, norm_r = 0;
    for (int i = 3; i < 6; i++) {
        norm_l += left_imu[i] * left_imu[i];
        norm_r += right_imu[i] * right_imu[i];
    }
    out[4] = sqrtf(norm_l) - sqrtf(norm_r);
    // out[5] = max gyro axis difference
    float max_diff = 0; int max_idx = 0;
    for (int i = 3; i < 6; i++) {
        float diff = fabsf(left_imu[i] - right_imu[i]);
        if (diff > max_diff) { max_diff = diff; max_idx = i - 3; }
    }
    out[5] = max_idx / 3.0f;
}

void test_assemble_28dim_from_pair() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 1, 0.1f, 1.0f));
    pairer.feed(make_pkt(HAND_RIGHT, 1, 0.5f, 2.0f));
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));

    float relative[6];
    compute_relative_from_imu(pair.left.imu, pair.right.imu, relative);

    float features[DUAL_HAND_FEATURES]; // 28
    memcpy(features, pair.left.flex, 5 * sizeof(float));
    memcpy(features + 5, pair.left.imu, 6 * sizeof(float));
    memcpy(features + 11, pair.right.flex, 5 * sizeof(float));
    memcpy(features + 16, pair.right.imu, 6 * sizeof(float));
    memcpy(features + 22, relative, 6 * sizeof(float));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, features[0]);   // left flex[0]
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, features[1]);   // left flex[1]
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, features[11]);  // right flex[0]
    TEST_ASSERT_NOT_EQUAL(0.0f, features[22]);              // delta euler != 0
}

void test_compute_relative_from_imu_delta_euler() {
    float left_imu[6]  = {10.0f, 20.0f, 30.0f, 1.0f, 2.0f, 3.0f};
    float right_imu[6] = {5.0f, 10.0f, 15.0f, 0.5f, 1.0f, 1.5f};
    float out[6];
    compute_relative_from_imu(left_imu, right_imu, out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, out[0]);   // 10-5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, out[1]);  // 20-10
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, out[2]);  // 30-15
}

void test_compute_relative_from_imu_zero_when_identical() {
    float imu[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    float out[6];
    compute_relative_from_imu(imu, imu, out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[3]); // euler dist = 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[4]); // gyro norm diff = 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_assemble_28dim_from_pair);
    RUN_TEST(test_compute_relative_from_imu_delta_euler);
    RUN_TEST(test_compute_relative_from_imu_zero_when_identical);
    return UNITY_END();
}
