/* =============================================================================
 * EchoGlove V5 — ESP-NOW Transmitter Stub
 * =============================================================================
 * Stub implementation of ESP-NOW packet transmission for unit testing.
 * Stores last sent packet and tracks send count for test assertions.
 *
 * Production version will use esp_now_send() with peer management.
 * =============================================================================
 */

#pragma once
#include "data_structures.h"
#include <cstdint>

class ESPNOWTransmitter {
public:
    bool begin(HandID hand_id) {
        hand_id_ = hand_id;
        initialized_ = true;
        return true;
    }

    bool send(const GlovePacket& pkt) {
        if (!initialized_) return false;
        last_sent_ = pkt;
        send_count_++;
        return true;
    }

    uint32_t getSendCount() const { return send_count_; }
    const GlovePacket& getLastSent() const { return last_sent_; }

private:
    bool initialized_ = false;
    HandID hand_id_ = HAND_LEFT;
    uint32_t send_count_ = 0;
    GlovePacket last_sent_ = {};
};
