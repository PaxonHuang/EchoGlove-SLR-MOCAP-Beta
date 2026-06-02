/* =============================================================================
 * EchoGlove V5 — ESP-NOW Transmitter Stub Tests
 * =============================================================================
 * Tests for the ESPNOWTransmitter stub and GlovePacket layout/checksum.
 * Runs on native platform (pio test -e native -f test_espnow).
 * =============================================================================
 */

#include <unity.h>
#include "ESPNOWTransmitter.h"

ESPNOWTransmitter tx;

void test_begin() { TEST_ASSERT_TRUE(tx.begin(HAND_LEFT)); }

void test_send_packet() {
    tx.begin(HAND_LEFT);
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5; pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 1; pkt.timestamp_us = 1000000;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    TEST_ASSERT_TRUE(tx.send(pkt));
    TEST_ASSERT_EQUAL(1, tx.getSendCount());
}

void test_packet_69_bytes() { TEST_ASSERT_EQUAL(69, sizeof(GlovePacket)); }

void test_checksum_roundtrip() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();
    TEST_ASSERT_TRUE(pkt.verifyChecksum());
    pkt.flex[0] = 0.99f;
    TEST_ASSERT_FALSE(pkt.verifyChecksum());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin);
    RUN_TEST(test_send_packet);
    RUN_TEST(test_packet_69_bytes);
    RUN_TEST(test_checksum_roundtrip);
    return UNITY_END();
}
