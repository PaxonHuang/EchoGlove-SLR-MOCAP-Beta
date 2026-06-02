#include "unity.h"
#include "ESPNOWReceiver.h"

void setUp() {}
void tearDown() {}

void test_receiver_parses_valid_packet() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5; pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 42; pkt.timestamp_us = 100000;
    pkt.flex[0] = 0.5f; pkt.imu[0] = 10.0f;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_TRUE(rx.receive(out));
    TEST_ASSERT_EQUAL(HAND_LEFT, out.hand_id);
    TEST_ASSERT_EQUAL(42, out.tick_id);
}

void test_receiver_rejects_bad_magic() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x00; pkt.magic[1] = 0x00;
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

void test_receiver_rejects_bad_checksum() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5; pkt.checksum = 0xDEAD;
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

void test_receiver_rejects_wrong_version() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 3; pkt.computeChecksum();
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_receiver_parses_valid_packet);
    RUN_TEST(test_receiver_rejects_bad_magic);
    RUN_TEST(test_receiver_rejects_bad_checksum);
    RUN_TEST(test_receiver_rejects_wrong_version);
    return UNITY_END();
}
