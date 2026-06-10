#include <unity.h>
#include "data_structures.h"
#include "FramePairer.h"

static GlovePacket make_pkt(uint8_t hand, uint32_t tick) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = hand;
    pkt.tick_id = tick;
    pkt.timestamp_us = tick * 10000;
    for (int i = 0; i < 5; i++) pkt.flex[i] = 0.1f * (i + 1);
    for (int i = 0; i < 6; i++) pkt.imu[i] = 0.5f * (i + 1);
    pkt.computeChecksum();
    return pkt;
}

void test_pairer_matches_by_tick_id() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.feed(make_pkt(HAND_RIGHT, 100));
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL_UINT32(100, pair.tick_id);
}

void test_pairer_rejects_mismatched_ticks() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.feed(make_pkt(HAND_RIGHT, 200));
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

void test_pairer_timeout() {
    FramePairer pairer(50); // 50ms timeout
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.tick(60000); // 60ms later
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
    pairer.feed(make_pkt(HAND_RIGHT, 100));
    TEST_ASSERT_FALSE(pairer.getPair(pair)); // left expired
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pairer_matches_by_tick_id);
    RUN_TEST(test_pairer_rejects_mismatched_ticks);
    RUN_TEST(test_pairer_timeout);
    return UNITY_END();
}
