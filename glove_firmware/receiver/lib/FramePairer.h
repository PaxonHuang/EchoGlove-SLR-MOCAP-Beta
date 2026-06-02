#pragma once
#include "data_structures.h"
#include <optional>

struct FramePair {
    uint32_t tick_id;
    GlovePacket left;
    GlovePacket right;
};

class FramePairer {
public:
    explicit FramePairer(uint32_t timeout_ms = 50) : timeout_us_(timeout_ms * 1000) {}

    void feed(const GlovePacket& pkt) {
        if (pkt.hand_id == HAND_LEFT) {
            left_pending_ = pkt;
            left_time_ = pkt.timestamp_us;
        } else {
            right_pending_ = pkt;
            right_time_ = pkt.timestamp_us;
        }
    }

    bool getPair(FramePair& out) {
        if (!left_pending_ || !right_pending_) return false;
        if (left_pending_->tick_id != right_pending_->tick_id) return false;
        out.tick_id = left_pending_->tick_id;
        out.left = *left_pending_;
        out.right = *right_pending_;
        left_pending_.reset();
        right_pending_.reset();
        return true;
    }

    void tick(uint32_t current_us) {
        if (left_pending_ && (current_us - left_time_) > timeout_us_)
            left_pending_.reset();
        if (right_pending_ && (current_us - right_time_) > timeout_us_)
            right_pending_.reset();
    }

private:
    uint32_t timeout_us_;
    std::optional<GlovePacket> left_pending_;
    std::optional<GlovePacket> right_pending_;
    uint32_t left_time_ = 0;
    uint32_t right_time_ = 0;
};
