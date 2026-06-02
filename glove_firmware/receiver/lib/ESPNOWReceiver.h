#pragma once
#include "data_structures.h"
#include <cstdint>
#include <queue>

class ESPNOWReceiver {
public:
    bool begin() { initialized_ = true; return true; }

    bool receive(GlovePacket& out) {
        if (rx_queue_.empty()) return false;
        out = rx_queue_.front();
        rx_queue_.pop();
        return true;
    }

    bool hasData() const { return !rx_queue_.empty(); }

    void injectPacket(const GlovePacket& pkt) {
        if (pkt.magic[0] != 0x45 || pkt.magic[1] != 0x47) return;
        if (pkt.version != 5) return;
        if (!pkt.verifyChecksum()) return;
        rx_queue_.push(pkt);
    }

private:
    bool initialized_ = false;
    std::queue<GlovePacket> rx_queue_;
};
