/* =============================================================================
 * InferenceTrigger — Confidence gating + debouncing for L1 gesture output
 * =============================================================================
 * SOP Deliverable F:
 *   - Confidence threshold: 0.85
 *   - Debouncing: 5 consecutive frames with same gesture ID + confidence >= 0.85
 *   - Silent period: 100ms after gesture change (blocks confirmation)
 *   - Same-gesture re-confirm: immediate (bypasses silent period)
 * =============================================================================
 */
#ifndef INFERENCE_TRIGGER_H
#define INFERENCE_TRIGGER_H

#include <cstdint>

class InferenceTrigger {
public:
    InferenceTrigger() = default;

    bool update(float confidence, int gesture_id, uint32_t timestamp_us) {
        // Already confirmed — same gesture stays emitted
        if (_confirmed && gesture_id == _current_id) {
            return true;
        }

        // Gesture transition while confirmed — enter silent period
        if (_confirmed && gesture_id != _current_id) {
            _confirmed = false;
            _pending_id = -1;
            _debounce_count = 0;
        }

        // Silent period blocks new confirmation
        if (_in_silent_period(timestamp_us)) {
            _pending_id = -1;
            _debounce_count = 0;
            return false;
        }

        // Confidence gate
        if (confidence < CONF_THRESHOLD) {
            _pending_id = -1;
            _debounce_count = 0;
            return false;
        }

        // Debounce tracking
        if (gesture_id == _pending_id) {
            _debounce_count++;
        } else {
            _pending_id = gesture_id;
            _debounce_count = 1;
        }

        // Confirm on reaching debounce threshold
        if (_debounce_count >= DEBOUNCE_FRAMES) {
            _confirmed = true;
            _current_id = gesture_id;
            _last_confirm_us = timestamp_us;
            return true;
        }

        return false;
    }

    bool confirmed() const { return _confirmed; }
    int currentGestureId() const { return _confirmed ? _current_id : -1; }
    int pendingGestureId() const { return _pending_id; }
    int debounceCount() const { return _debounce_count; }

    void reset() {
        _confirmed = false;
        _current_id = -1;
        _pending_id = -1;
        _debounce_count = 0;
        _last_confirm_us = 0;
    }

private:
    static constexpr float CONF_THRESHOLD = 0.85f;
    static constexpr int DEBOUNCE_FRAMES = 5;
    static constexpr uint32_t SILENT_PERIOD_US = 100000;  // 100ms

    bool _confirmed = false;
    int _current_id = -1;
    int _pending_id = -1;
    int _debounce_count = 0;
    uint32_t _last_confirm_us = 0;

    bool _in_silent_period(uint32_t timestamp_us) const {
        if (_last_confirm_us == 0) return false;
        return (timestamp_us - _last_confirm_us) < SILENT_PERIOD_US;
    }
};

#endif
