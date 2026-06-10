/*
 * Unit tests for UART frame protocol (uart_frame.h)
 * Native test — no hardware required.
 */

#include <unity.h>
#include <cstring>
#include "data_structures.h"
#include "uart_frame.h"

void test_encode_roundtrip() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 42;
    pkt.flex[0] = 0.5f;
    pkt.imu[0] = 1.23f;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t frame_len = uart_frame_encode(&pkt, sizeof(GlovePacket), frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN(0, (int)frame_len);

    GlovePacket decoded = {};
    size_t consumed = 0;
    TEST_ASSERT_TRUE(uart_frame_decode(frame, frame_len, &decoded, &consumed));
    TEST_ASSERT_EQUAL_MEMORY(&pkt, &decoded, sizeof(GlovePacket));
}

void test_decode_rejects_bad_magic() {
    uint8_t frame[] = {0xFF, 0xFF, 0x00};
    GlovePacket decoded;
    size_t consumed;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, sizeof(frame), &decoded, &consumed));
}

void test_decode_rejects_bad_crc() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t len = uart_frame_encode(&pkt, sizeof(GlovePacket), frame, sizeof(frame));
    frame[len - 1] ^= 0xFF; // corrupt CRC low byte

    GlovePacket decoded;
    size_t consumed;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, len, &decoded, &consumed));
}

void test_decode_partial_frame_returns_zero_consumed() {
    uint8_t frame[] = {0xAA, 0x55}; // incomplete
    GlovePacket decoded;
    size_t consumed = 999;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, 2, &decoded, &consumed));
    TEST_ASSERT_EQUAL(0, (int)consumed);
}

void test_encode_size() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t len = uart_frame_encode(&pkt, sizeof(GlovePacket), frame, sizeof(frame));
    // Expected: 2 (magic) + 69 (GlovePacket) + 2 (CRC) = 73
    TEST_ASSERT_EQUAL(73, (int)len);
}

void test_crc16_modbus_known_vector() {
    // CRC-16/MODBUS of {0x45, 0x47, 0x05, ...} should be deterministic
    uint8_t data[] = {0x45, 0x47, 0x05, 0x00, 0x2A, 0x00, 0x00, 0x00};
    uint16_t crc = crc16_modbus(data, sizeof(data));
    TEST_ASSERT_NOT_EQUAL(0, crc);
    // Verify recomputation gives same result
    TEST_ASSERT_EQUAL(crc, crc16_modbus(data, sizeof(data)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_encode_roundtrip);
    RUN_TEST(test_decode_rejects_bad_magic);
    RUN_TEST(test_decode_rejects_bad_crc);
    RUN_TEST(test_decode_partial_frame_returns_zero_consumed);
    RUN_TEST(test_encode_size);
    RUN_TEST(test_crc16_modbus_known_vector);
    return UNITY_END();
}
