/* =============================================================================
 * EchoGlove V5.2 — UART Frame Protocol
 * =============================================================================
 * Lightweight framing layer for C6 <-> P4 UART communication at 2 Mbps.
 *
 * Frame format:
 *   [0xAA][0x55][Payload (69 bytes)][CRC16_H][CRC16_L]
 *   Total: 2 + 69 + 2 = 73 bytes
 *
 * CRC-16/MODBUS is computed over the payload bytes only (same algorithm as
 * GlovePacket::computeChecksum, but here applied to the raw payload).
 *
 * This header is pure C++ with no hardware dependencies — testable natively.
 * =============================================================================
 */

#ifndef UART_FRAME_H
#define UART_FRAME_H

#include <cstdint>
#include <cstring>

// ── Constants ────────────────────────────────────────────────────
static constexpr uint8_t  UART_MAGIC_0        = 0xAA;
static constexpr uint8_t  UART_MAGIC_1        = 0x55;
static constexpr size_t   UART_FRAME_MAX_SIZE = 73;  // 2 + 69 + 2

// ── CRC-16/MODBUS ───────────────────────────────────────────────
/**
 * Compute CRC-16/MODBUS (polynomial 0xA001, init 0xFFFF).
 * Same algorithm as GlovePacket::computeChecksum().
 */
inline uint16_t crc16_modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

// ── Encode ───────────────────────────────────────────────────────
/**
 * Wrap a payload (e.g. GlovePacket) into a UART frame.
 *
 * @param payload     Pointer to the payload bytes.
 * @param payload_len Number of bytes in the payload (typically 69).
 * @param out_frame   Output buffer for the framed data.
 * @param out_max     Size of the output buffer.
 * @return            Total frame length written, or 0 on error (buffer too small).
 */
inline size_t uart_frame_encode(const void* payload, size_t payload_len,
                                 uint8_t* out_frame, size_t out_max) {
    size_t frame_len = 2 + payload_len + 2;
    if (out_max < frame_len) return 0;

    out_frame[0] = UART_MAGIC_0;
    out_frame[1] = UART_MAGIC_1;
    memcpy(out_frame + 2, payload, payload_len);
    uint16_t crc = crc16_modbus(static_cast<const uint8_t*>(payload), payload_len);
    out_frame[2 + payload_len]     = crc & 0xFF;        // CRC low byte
    out_frame[2 + payload_len + 1] = (crc >> 8) & 0xFF; // CRC high byte
    return frame_len;
}

// ── Decode ───────────────────────────────────────────────────────
/**
 * Decode a UART frame back into a payload.
 *
 * @param data         Raw byte stream from UART.
 * @param data_len     Number of bytes available in the stream.
 * @param out_payload  Output buffer for the decoded payload (at least 69 bytes).
 * @param out_consumed On return: number of bytes consumed from data (0 if decode failed).
 * @return             true if a valid frame was decoded, false otherwise.
 *
 * A partial frame (insufficient data) returns false with *out_consumed = 0,
 * allowing the caller to buffer and retry when more data arrives.
 */
inline bool uart_frame_decode(const uint8_t* data, size_t data_len,
                               void* out_payload, size_t* out_consumed) {
    *out_consumed = 0;

    // Need at least magic + 1 byte to even start
    if (data_len < 2) return false;

    // Check magic bytes
    if (data[0] != UART_MAGIC_0 || data[1] != UART_MAGIC_1) return false;

    // Fixed payload size: GlovePacket = 69 bytes
    constexpr size_t PAYLOAD_SIZE = 69;
    constexpr size_t FRAME_LEN = 2 + PAYLOAD_SIZE + 2;  // 73

    if (data_len < FRAME_LEN) return false;  // partial frame, not enough data

    // Copy payload
    memcpy(out_payload, data + 2, PAYLOAD_SIZE);

    // Verify CRC
    uint16_t expected_crc = static_cast<uint16_t>(data[2 + PAYLOAD_SIZE]) |
                            (static_cast<uint16_t>(data[2 + PAYLOAD_SIZE + 1]) << 8);
    uint16_t actual_crc = crc16_modbus(data + 2, PAYLOAD_SIZE);
    if (expected_crc != actual_crc) return false;

    *out_consumed = FRAME_LEN;
    return true;
}

#endif // UART_FRAME_H
