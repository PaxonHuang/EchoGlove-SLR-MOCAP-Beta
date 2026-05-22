/* =============================================================================
 * EdgeAI Data Glove V3 — Inference Pipeline Tests (TDD RED Phase)
 * =============================================================================
 * Tests for runInferencePipeline() — the glue between SlidingWindow,
 * BaseModel, and InferenceTrigger.
 *
 * Pipeline flow:
 *   SlidingWindow (full) → model.preprocess() → model.infer()
 *   → model.postprocess() → InferenceTrigger.update() → confirmed GestureResult
 * =============================================================================
 */

#include <unity.h>
#include <cstring>
#include <cmath>

void setup() {}
void loop() {}

#include "data_structures.h"
#include "SlidingWindow.h"
#include "InferenceTrigger.h"
#include "MockModel.h"

// The function under test — doesn't exist yet (RED)
#include "InferencePipeline.h"

// Test fixtures
static MockModel* model = nullptr;
static SlidingWindow* window = nullptr;
static InferenceTrigger* trigger = nullptr;

void setUp(void) {
    model = new MockModel();
    model->init(nullptr, 0);
    window = new SlidingWindow();
    trigger = new InferenceTrigger();
}

void tearDown(void) {
    if (model) { model->cleanup(); delete model; model = nullptr; }
    if (window) { delete window; window = nullptr; }
    if (trigger) { delete trigger; trigger = nullptr; }
}

// Helper: fill sliding window with synthetic frames
static void fillWindow(SlidingWindow* w, int gesture_id) {
    float features[FEATURE_COUNT];
    for (int f = 0; f < WINDOW_SIZE; f++) {
        memset(features, 0, sizeof(features));
        // Set hall_xyz pattern based on gesture
        for (int i = 0; i < HALL_FEATURE_COUNT; i++) {
            features[i] = (gesture_id * 0.1f) + (i * 0.01f);
        }
        w->push(features);
    }
}

// =============================================================================
// Test 1: Pipeline returns false when window not full
// =============================================================================

void test_pipeline_window_not_full(void) {
    // Push only 10 frames (not full)
    float features[FEATURE_COUNT] = {};
    for (int i = 0; i < 10; i++) {
        window->push(features);
    }

    GestureResult result;
    result.zero();
    bool confirmed = runInferencePipeline(*window, *model, *trigger, result);

    TEST_ASSERT_FALSE(confirmed);
    TEST_ASSERT_FALSE(result.valid);
}

// =============================================================================
// Test 2: Pipeline confirms after 5 debounce frames (high confidence)
// =============================================================================

void test_pipeline_confirms_after_debounce(void) {
    model->setMockOutput(3, 0.95f);
    fillWindow(window, 3);

    GestureResult result;
    bool confirmed = false;

    // Run pipeline 5 times (simulates 5 frames at 100Hz = 50ms)
    uint32_t t = 100000;
    for (int i = 0; i < 4; i++) {
        confirmed = runInferencePipeline(*window, *model, *trigger, result, t);
        TEST_ASSERT_FALSE(confirmed);  // Not yet debounced
        t += 10000;
    }

    confirmed = runInferencePipeline(*window, *model, *trigger, result, t);
    TEST_ASSERT_TRUE(confirmed);
    TEST_ASSERT_EQUAL(3, result.gesture_id);
}

// =============================================================================
// Test 3: Pipeline returns false with low confidence
// =============================================================================

void test_pipeline_low_confidence_no_confirm(void) {
    model->setMockOutput(5, 0.40f);  // In uncertain band → l2_requested
    fillWindow(window, 5);

    GestureResult result;
    bool confirmed = false;

    // Run 10 times — should never confirm (confidence too low for debounce)
    uint32_t t = 100000;
    for (int i = 0; i < 10; i++) {
        confirmed = runInferencePipeline(*window, *model, *trigger, result, t);
        t += 10000;
    }

    TEST_ASSERT_FALSE(confirmed);
}

// =============================================================================
// Test 4: Pipeline handles gesture transition with silent period
// =============================================================================

void test_pipeline_gesture_transition_silent_period(void) {
    // Confirm gesture 3
    model->setMockOutput(3, 0.95f);
    fillWindow(window, 3);

    GestureResult result;
    uint32_t t = 100000;
    for (int i = 0; i < 5; i++) {
        runInferencePipeline(*window, *model, *trigger, result, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(trigger->confirmed());
    TEST_ASSERT_EQUAL(3, trigger->currentGestureId());

    // Switch to gesture 7 — in silent period
    model->setMockOutput(7, 0.95f);

    bool confirmed = false;
    // During 100ms silent period, should not confirm
    for (int i = 0; i < 5; i++) {
        confirmed = runInferencePipeline(*window, *model, *trigger, result, t);
        t += 10000;
    }
    TEST_ASSERT_FALSE(confirmed);

    // After silent period, should confirm
    t += 60000;  // Past 100ms
    for (int i = 0; i < 5; i++) {
        confirmed = runInferencePipeline(*window, *model, *trigger, result, t);
        t += 10000;
    }
    TEST_ASSERT_TRUE(confirmed);
    TEST_ASSERT_EQUAL(7, result.gesture_id);
}

// =============================================================================
// Test 5: Pipeline passes through l2_requested from model
// =============================================================================

void test_pipeline_l2_request_passthrough(void) {
    model->setMockOutput(2, 0.45f);  // l2_requested band
    fillWindow(window, 2);

    GestureResult result;
    result.zero();
    runInferencePipeline(*window, *model, *trigger, result, 100000);

    // Model says l2_requested, but gesture not confirmed (low confidence)
    // The result should carry the l2 flag for upstream handling
    TEST_ASSERT_TRUE(result.l2_requested);
}

// =============================================================================
// Test 6: Pipeline with unready model returns false
// =============================================================================

void test_pipeline_unready_model(void) {
    model->cleanup();  // model no longer ready
    fillWindow(window, 0);

    GestureResult result;
    result.zero();
    bool confirmed = runInferencePipeline(*window, *model, *trigger, result);

    TEST_ASSERT_FALSE(confirmed);
    TEST_ASSERT_FALSE(result.valid);
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_pipeline_window_not_full);
    RUN_TEST(test_pipeline_confirms_after_debounce);
    RUN_TEST(test_pipeline_low_confidence_no_confirm);
    RUN_TEST(test_pipeline_gesture_transition_silent_period);
    RUN_TEST(test_pipeline_l2_request_passthrough);
    RUN_TEST(test_pipeline_unready_model);

    UNITY_END();
}
