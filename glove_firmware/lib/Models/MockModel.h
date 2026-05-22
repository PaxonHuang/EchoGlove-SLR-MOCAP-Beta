/* =============================================================================
 * MockModel — Configurable BaseModel for pipeline testing
 * =============================================================================
 * No TFLite dependency. Outputs a configurable gesture + confidence.
 * Use setMockOutput(gesture_id, confidence) to control what the model predicts.
 * =============================================================================
 */
#ifndef MOCK_MODEL_H
#define MOCK_MODEL_H

#include "BaseModel.h"
#include <cmath>

class MockModel : public BaseModel {
public:
    MockModel() = default;
    ~MockModel() override { cleanup(); }

    // ---- Lifecycle ----

    bool init(const uint8_t*, size_t) override {
        _ready = true;
        return true;
    }

    void cleanup() override {
        _ready = false;
    }

    // ---- Pipeline ----

    int preprocess(float* input, const SensorData* frames, int num_frames) override {
        if (!input || !frames || num_frames <= 0) return -1;
        int count = 0;
        for (int i = 0; i < num_frames; i++) {
            frames[i].toFeatureArray(input + i * FEATURE_COUNT);
            count += FEATURE_COUNT;
        }
        return count;
    }

    int infer(const float* /*input*/, float* output) override {
        if (!output) return -1;
        // Fill with small baseline logits
        for (int i = 0; i < NUM_CLASSES; i++) {
            output[i] = 0.0f;
        }
        // Give mock gesture a high logit (~3.0) so softmax yields ~0.95
        output[_mock_gesture_id] = 3.0f;
        return NUM_CLASSES;
    }

    int postprocess(const float* output, GestureResult* result) override {
        if (!output || !result) return -1;

        // Softmax
        float max_val = output[0];
        int max_idx = 0;
        for (int i = 1; i < NUM_CLASSES; i++) {
            if (output[i] > max_val) {
                max_val = output[i];
                max_idx = i;
            }
        }

        float sum = 0.0f;
        for (int i = 0; i < NUM_CLASSES; i++) {
            result->scores[i] = expf(output[i] - max_val);
            sum += result->scores[i];
        }
        for (int i = 0; i < NUM_CLASSES; i++) {
            result->scores[i] /= sum;
        }

        result->gesture_id = max_idx;
        result->confidence = result->scores[max_idx];

        // Override confidence with mock value for deterministic testing
        result->confidence = _mock_confidence;

        // Validity gates
        result->valid = (result->confidence >= 0.6f);
        result->l2_requested = (result->confidence >= 0.3f && result->confidence < 0.6f);

        return 0;
    }

    // ---- Metadata ----

    ModelInfo get_model_info() const override {
        ModelInfo info;
        snprintf(info.name, sizeof(info.name), "MockModel");
        snprintf(info.type, sizeof(info.type), "Mock");
        info.input_features = FEATURE_COUNT;
        info.window_size = WINDOW_SIZE;
        info.num_classes = NUM_CLASSES;
        info.model_size_bytes = 0;
        info.arena_size_bytes = 0;
        return info;
    }

    const char* name() const override { return "MockModel"; }
    bool isReady() const override { return _ready; }
    uint32_t lastInferenceTimeUs() const override { return 0; }

    // ---- Mock Configuration ----

    void setMockOutput(int gesture_id, float confidence) {
        _mock_gesture_id = gesture_id;
        _mock_confidence = confidence;
    }

private:
    bool _ready = false;
    int _mock_gesture_id = 0;
    float _mock_confidence = 0.95f;
};

#endif
