/* =============================================================================
 * EdgeAI Data Glove V3 — MockModel Tests (TDD RED Phase)
 * =============================================================================
 * Tests for MockModel — a configurable BaseModel implementation for
 * pipeline testing without real TFLite model data.
 *
 * MockModel features:
 *   - Configurable output gesture ID + confidence
 *   - preprocess() extracts 21 features from SensorData window
 *   - infer() produces configurable logits
 *   - postprocess() applies softmax + argmax
 *   - Works in native test environment (no Arduino/PSRAM needed)
 * =============================================================================
 */

#include <unity.h>
#include <cstring>
#include <cmath>

// Arduino framework requires setup/loop — stubs for test binary
void setup() {}
void loop() {}

#include "data_structures.h"
#include "BaseModel.h"
#include "MockModel.h"

static MockModel* model = nullptr;

void setUp(void) {
    model = new MockModel();
}

void tearDown(void) {
    if (model) {
        model->cleanup();
        delete model;
        model = nullptr;
    }
}

// =============================================================================
// Test 1: init() sets ready state
// =============================================================================

void test_init_sets_ready(void) {
    // MockModel init should succeed with any data (or nullptr)
    bool ok = model->init(nullptr, 0);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(model->isReady());
}

// =============================================================================
// Test 2: name() returns "MockModel"
// =============================================================================

void test_name_returns_mock_model(void) {
    model->init(nullptr, 0);
    TEST_ASSERT_EQUAL_STRING("MockModel", model->name());
}

// =============================================================================
// Test 3: get_model_info() returns correct dimensions
// =============================================================================

void test_model_info_dimensions(void) {
    model->init(nullptr, 0);
    ModelInfo info = model->get_model_info();

    TEST_ASSERT_EQUAL(FEATURE_COUNT, info.input_features);  // 21
    TEST_ASSERT_EQUAL(WINDOW_SIZE, info.window_size);       // 30
    TEST_ASSERT_EQUAL(NUM_CLASSES, info.num_classes);        // 46
}

// =============================================================================
// Test 4: preprocess() extracts features from SensorData window
// =============================================================================

void test_preprocess_extracts_features(void) {
    model->init(nullptr, 0);

    // Create 30 frames with known values
    SensorData frames[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        frames[i].zero();
        // Set hall_xyz[0] = i * 0.1f (distinct per frame)
        frames[i].hall_xyz[0] = i * 0.1f;
        // Set euler[0] = i * 2.0f
        frames[i].euler[0] = i * 2.0f;
    }

    float input[WINDOW_SIZE * FEATURE_COUNT];
    int written = model->preprocess(input, frames, WINDOW_SIZE);

    TEST_ASSERT_EQUAL(WINDOW_SIZE * FEATURE_COUNT, written);

    // Verify first frame: hall_xyz[0] = 0.0, euler[0] = 0.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, input[0]);

    // Verify frame 5: hall_xyz[0] = 0.5, euler[0] = 10.0
    int frame5_start = 5 * FEATURE_COUNT;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, input[frame5_start]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, input[frame5_start + HALL_FEATURE_COUNT]);
}

// =============================================================================
// Test 5: infer() produces configurable output
// =============================================================================

void test_infer_configurable_output(void) {
    model->init(nullptr, 0);

    // Configure MockModel to output gesture 7 with 0.92 confidence
    model->setMockOutput(7, 0.92f);

    float input[WINDOW_SIZE * FEATURE_COUNT];
    memset(input, 0, sizeof(input));

    float output[NUM_CLASSES];
    int written = model->infer(input, output);

    TEST_ASSERT_EQUAL(NUM_CLASSES, written);

    // Output[7] should be highest (pre-softmax logit)
    TEST_ASSERT_TRUE(output[7] > output[0]);
    TEST_ASSERT_TRUE(output[7] > output[1]);
}

// =============================================================================
// Test 6: postprocess() converts logits to GestureResult
// =============================================================================

void test_postprocess_softmax_argmax(void) {
    model->init(nullptr, 0);
    model->setMockOutput(3, 0.95f);

    float input[WINDOW_SIZE * FEATURE_COUNT];
    memset(input, 0, sizeof(input));
    float output[NUM_CLASSES];
    model->infer(input, output);

    GestureResult result;
    result.zero();
    int ret = model->postprocess(output, &result);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(3, result.gesture_id);
    TEST_ASSERT_TRUE(result.confidence > 0.9f);
    TEST_ASSERT_TRUE(result.valid);  // confidence > threshold
}

// =============================================================================
// Test 7: low confidence triggers l2_requested
// =============================================================================

void test_low_confidence_triggers_l2(void) {
    model->init(nullptr, 0);

    // Set low confidence in uncertain band [0.3, 0.6)
    model->setMockOutput(5, 0.45f);

    float input[WINDOW_SIZE * FEATURE_COUNT];
    memset(input, 0, sizeof(input));
    float output[NUM_CLASSES];
    model->infer(input, output);

    GestureResult result;
    result.zero();
    model->postprocess(output, &result);

    TEST_ASSERT_EQUAL(5, result.gesture_id);
    TEST_ASSERT_TRUE(result.l2_requested);
}

// =============================================================================
// Test 8: very low confidence produces invalid result
// =============================================================================

void test_very_low_confidence_invalid(void) {
    model->init(nullptr, 0);

    model->setMockOutput(2, 0.1f);

    float input[WINDOW_SIZE * FEATURE_COUNT];
    memset(input, 0, sizeof(input));
    float output[NUM_CLASSES];
    model->infer(input, output);

    GestureResult result;
    result.zero();
    model->postprocess(output, &result);

    TEST_ASSERT_FALSE(result.valid);
}

// =============================================================================
// Test 9: cleanup() resets ready state
// =============================================================================

void test_cleanup_resets_ready(void) {
    model->init(nullptr, 0);
    TEST_ASSERT_TRUE(model->isReady());

    model->cleanup();
    TEST_ASSERT_FALSE(model->isReady());
}

// =============================================================================
// Test 10: lastInferenceTimeUs returns 0 for mock
// =============================================================================

void test_inference_time_zero_for_mock(void) {
    model->init(nullptr, 0);

    float input[WINDOW_SIZE * FEATURE_COUNT];
    memset(input, 0, sizeof(input));
    float output[NUM_CLASSES];
    model->infer(input, output);

    TEST_ASSERT_EQUAL_UINT32(0, model->lastInferenceTimeUs());
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_sets_ready);
    RUN_TEST(test_name_returns_mock_model);
    RUN_TEST(test_model_info_dimensions);
    RUN_TEST(test_preprocess_extracts_features);
    RUN_TEST(test_infer_configurable_output);
    RUN_TEST(test_postprocess_softmax_argmax);
    RUN_TEST(test_low_confidence_triggers_l2);
    RUN_TEST(test_very_low_confidence_invalid);
    RUN_TEST(test_cleanup_resets_ready);
    RUN_TEST(test_inference_time_zero_for_mock);

    UNITY_END();
}
