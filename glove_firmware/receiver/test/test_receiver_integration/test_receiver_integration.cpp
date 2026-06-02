#include "unity.h"
#include "ESPNOWReceiver.h"
#include "FramePairer.h"
#include "RelativeFeatures.h"

void setUp() {}
void tearDown() {}

GlovePacket makePkt(HandID hand, uint32_t tick, float flex0, float euler0, float gx, float gy, float gz) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5; pkt.hand_id = hand;
    pkt.tick_id = tick; pkt.timestamp_us = 100000;
    pkt.flex[0] = flex0;
    pkt.imu[0] = euler0;
    pkt.imu[3] = gx; pkt.imu[4] = gy; pkt.imu[5] = gz;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    return pkt;
}

void test_full_receiver_pipeline() {
    ESPNOWReceiver rx;
    FramePairer pairer;
    RelativeFeatures rf;
    rx.begin();

    rx.injectPacket(makePkt(HAND_LEFT, 50, 0.3f, 10.0f, 1, 2, 3));
    rx.injectPacket(makePkt(HAND_RIGHT, 50, 0.7f, 20.0f, 4, 5, 6));

    GlovePacket p1, p2;
    TEST_ASSERT_TRUE(rx.receive(p1));
    TEST_ASSERT_TRUE(rx.receive(p2));

    pairer.feed(p1);
    pairer.feed(p2);
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL(50, pair.tick_id);

    float rel[6];
    // GlovePacket.imu[0..2] = euler, imu[3..5] = gyro; no quaternion field
    rf.compute(&pair.left.imu[0], nullptr,
               &pair.right.imu[0], nullptr,
               rel, &pair.left.imu[3], &pair.right.imu[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, rel[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_receiver_pipeline);
    return UNITY_END();
}
