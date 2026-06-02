#include "unity.h"
#include "FramePairer.h"

void setUp() {}
void tearDown() {}

GlovePacket makePacket(HandID hand, uint32_t tick, uint32_t ts = 100000) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5; pkt.hand_id = hand;
    pkt.tick_id = tick; pkt.timestamp_us = ts;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    return pkt;
}

void test_pairer_matches_by_tick_id() {
    FramePairer pairer;
    pairer.feed(makePacket(HAND_LEFT, 100));
    pairer.feed(makePacket(HAND_RIGHT, 100));
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL(100, pair.tick_id);
}

void test_pairer_unmatched_returns_false() {
    FramePairer pairer;
    pairer.feed(makePacket(HAND_LEFT, 200));
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

void test_pairer_timeout_old_frames() {
    FramePairer pairer(10);
    pairer.feed(makePacket(HAND_LEFT, 300, 0));
    pairer.tick(100000);
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pairer_matches_by_tick_id);
    RUN_TEST(test_pairer_unmatched_returns_false);
    RUN_TEST(test_pairer_timeout_old_frames);
    return UNITY_END();
}
