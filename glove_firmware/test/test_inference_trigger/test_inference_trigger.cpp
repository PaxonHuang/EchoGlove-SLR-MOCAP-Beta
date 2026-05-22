/* =============================================================================
 * EdgeAI Data Glove V3 — Inference Trigger Tests (TDD RED Phase)
 * =============================================================================
 * Tests for confidence gating, debouncing, and gesture transition logic.
 *
 * SOP Deliverable F:
 *   - Confidence threshold: 0.85
 *   - Debouncing: 5 consecutive frames with same gesture ID + confidence > threshold
 *   - Gesture transition: 100ms silent period on gesture change
 *   - Unknown: confidence <= 0.85 → UNKNOWN gesture
 *
 * These tests are written BEFORE the InferenceTrigger class exists.
 * They MUST fail on first run (RED).
 * =============================================================================
 */

#include <unity.h>
#include <cstring>

// Arduino framework requires setup/loop — stubs for test binary
void setup() {}
void loop() {}

// InferenceTrigger does not exist yet — this include will FAIL (RED)
#include "InferenceTrigger.h"

// Test fixture
static InferenceTrigger* trigger = nullptr;

void setUp(void) {
    trigger = new InferenceTrigger();
}

void tearDown(void) {
    delete trigger;
    trigger = nullptr;
}

// =============================================================================
// Test 1: Initial state — no gesture confirmed
// =============================================================================

void test_initial_state_no_confirmed(void) {
    // Before any frames, confirmed() should be false
    TEST_ASSERT_FALSE(trigger->confirmed());
    TEST_ASSERT_EQUAL(-1, trigger->currentGestureId());
    TEST_ASSERT_EQUAL(0, trigger->debounceCount());
}

// =============================================================================
// Test 2: Single high-confidence frame — not yet confirmed (needs 5)
// =============================================================================

void test_single_high_confidence_not_confirmed(void) {
    // Push 1 frame at gesture 3 with 0.95 confidence
    bool emitted = trigger->update(0.95f, 3, 100000);  // t=100ms

    TEST_ASSERT_FALSE(emitted);
    TEST_ASSERT_FALSE(trigger->confirmed());
    TEST_ASSERT_EQUAL(3, trigger->pendingGestureId());
    TEST_ASSERT_EQUAL(1, trigger->debounceCount());
}

// =============================================================================
// Test 3: 5 consecutive high-confidence frames → confirmed
// =============================================================================

void test_five_consecutive_confirmed(void) {
    uint32_t t = 100000;  // Start at 100ms

    for (int i = 0; i < 4; i++) {
        bool emitted = trigger->update(0.90f, 5, t);
        TEST_ASSERT_FALSE(emitted);  // Not confirmed yet
        t += 10000;  // 10ms per frame (100Hz)
    }

    // 5th frame → should confirm
    bool emitted = trigger->update(0.90f, 5, t);
    TEST_ASSERT_TRUE(emitted);
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(5, trigger->currentGestureId());
}

// =============================================================================
// Test 4: Confidence below threshold resets debounce counter
// =============================================================================

void test_low_confidence_resets_debounce(void) {
    uint32_t t = 100000;

    // 4 high-confidence frames
    for (int i = 0; i < 4; i++) {
        trigger->update(0.90f, 2, t);
        t += 10000;
    }
    TEST_ASSERT_EQUAL(4, trigger->debounceCount());

    // 1 low-confidence frame → resets counter
    trigger->update(0.50f, 2, t);
    TEST_ASSERT_EQUAL(0, trigger->debounceCount());
    TEST_ASSERT_FALSE(trigger->confirmed());
}

// =============================================================================
// Test 5: Gesture ID change resets debounce counter
// =============================================================================

void test_gesture_change_resets_debounce(void) {
    uint32_t t = 100000;

    // 4 frames for gesture 3
    for (int i = 0; i < 4; i++) {
        trigger->update(0.90f, 3, t);
        t += 10000;
    }
    TEST_ASSERT_EQUAL(4, trigger->debounceCount());

    // Gesture switches to 7 → resets counter, starts new count for 7
    trigger->update(0.90f, 7, t);
    TEST_ASSERT_EQUAL(1, trigger->debounceCount());
    TEST_ASSERT_EQUAL(7, trigger->pendingGestureId());
    TEST_ASSERT_FALSE(trigger->confirmed());
}

// =============================================================================
// Test 6: Silent period after gesture change (100ms)
// =============================================================================

void test_silent_period_after_gesture_change(void) {
    uint32_t t = 100000;

    // Confirm gesture 3
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 3, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(3, trigger->currentGestureId());

    // Gesture changes to 7 immediately — should enter silent period
    // During silent period, confirmed() should be false even if gesture 7 is debounced
    // After 100ms silent period, gesture 7 can be confirmed

    // Push gesture 7 frames during silent period (< 100ms after last gesture 3 confirm)
    t += 50000;  // Only 50ms passed
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 7, t);
        t += 10000;
    }

    // Even though 5 frames of gesture 7, we're still in silent period
    TEST_ASSERT_FALSE(trigger->confirmed());

    // Wait another 60ms (total > 100ms since last confirm)
    t += 60000;
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 7, t);
        t += 10000;
    }

    // Now should confirm gesture 7
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(7, trigger->currentGestureId());
}

// =============================================================================
// Test 7: Same gesture repeated — immediate re-confirm
// =============================================================================

void test_same_gesture_reconfirm(void) {
    uint32_t t = 100000;

    // Confirm gesture 3
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 3, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());

    // Continue pushing same gesture — should stay confirmed
    for (int i = 0; i < 10; i++) {
        bool emitted = trigger->update(0.90f, 3, t);
        TEST_ASSERT_TRUE(emitted);
        TEST_ASSERT_EQUAL(3, trigger->currentGestureId());
        t += 10000;
    }
}

// =============================================================================
// Test 8: Reset clears all state
// =============================================================================

void test_reset_clears_state(void) {
    uint32_t t = 100000;

    // Confirm gesture 3
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 3, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());

    trigger->reset();

    TEST_ASSERT_FALSE(trigger->confirmed());
    TEST_ASSERT_EQUAL(-1, trigger->currentGestureId());
    TEST_ASSERT_EQUAL(0, trigger->debounceCount());
}

// =============================================================================
// Test 9: Confidence exactly at threshold (0.85) — should count
// =============================================================================

void test_confidence_at_threshold_counts(void) {
    uint32_t t = 100000;

    for (int i = 0; i < 5; i++) {
        bool emitted = trigger->update(0.85f, 1, t);
        t += 10000;
    }

    // 0.85 is >= threshold, should confirm
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(1, trigger->currentGestureId());
}

// =============================================================================
// Test 10: Confidence just below threshold (0.849) — should NOT count
// =============================================================================

void test_confidence_below_threshold_no_confirm(void) {
    uint32_t t = 100000;

    for (int i = 0; i < 10; i++) {
        trigger->update(0.849f, 1, t);
        t += 10000;
    }

    TEST_ASSERT_FALSE(trigger->confirmed());
}

// =============================================================================
// Test 11: Interleaved gestures — both eventually confirm with gap
// =============================================================================

void test_interleaved_gestures(void) {
    uint32_t t = 100000;

    // Confirm gesture 1
    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 1, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(1, trigger->currentGestureId());

    // Switch to gesture 2 — need silent period
    t += 110000;  // Past 100ms silent period

    for (int i = 0; i < 5; i++) {
        trigger->update(0.90f, 2, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(2, trigger->currentGestureId());
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_initial_state_no_confirmed);
    RUN_TEST(test_single_high_confidence_not_confirmed);
    RUN_TEST(test_five_consecutive_confirmed);
    RUN_TEST(test_low_confidence_resets_debounce);
    RUN_TEST(test_gesture_change_resets_debounce);
    RUN_TEST(test_silent_period_after_gesture_change);
    RUN_TEST(test_same_gesture_reconfirm);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_confidence_at_threshold_counts);
    RUN_TEST(test_confidence_below_threshold_no_confirm);
    RUN_TEST(test_interleaved_gestures);

    UNITY_END();
}
