/* =============================================================================
 * EdgeAI Data Glove V3 — TFLite Micro Model Implementation
 * =============================================================================
 * Concrete implementation of BaseModel using TensorFlow Lite for Microcontrollers.
 *
 * Key Design Decisions:
 *   1. Tensor arena allocated in PSRAM via ps_malloc() (8 MB available)
 *   2. Model binary loaded from PROGMEM (compiled-in header file)
 *   3. Supports both frame-wise and temporal (windowed) models
 *   4. Preprocessing: z-score normalization using per-feature mean/std
 *   5. Postprocessing: softmax + argmax + confidence thresholding
 *
 * Memory Layout:
 *   - Model binary: Flash (PROGMEM) — not copied to RAM
 *   - Tensor arena: PSRAM — ~300-500 KB depending on model size
 *   - Input/output buffers: PSRAM — allocated per-inference in run()
 *
 * Build Integration:
 *   - Model binary included via: #include "model_data_cnn_attn.h"
 *   - That header defines: const unsigned char model_data[] PROGMEM = { ... };
 *   - See platformio.ini: -DTFLITE_MODEL_ARENA_SIZE=512000
 *
 * V3 Note:
 *   In V2, the interpreter was a global singleton which prevented model
 *   hot-switching. V3 wraps it in this class so cleanup()/init() can
 *   properly release and re-allocate the interpreter.
 * =============================================================================
 */

#ifndef TFLITE_MODEL_H
#define TFLITE_MODEL_H

#include <Arduino.h>

// ESP32-S3 optimized TFLite Micro headers
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include "BaseModel.h"
#include "data_structures.h"

class TFLiteModel : public BaseModel {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @param model_name       Name identifier (matches model_config.yaml)
     * @param model_type       Type string (e.g., "CNNAttention", "MSTCN")
     * @param input_features   Expected feature count per frame (default 21)
     * @param window_size      Temporal window length (default 30)
     * @param num_classes      Output class count (default 46)
     * @param arena_size       Tensor arena size in bytes (default from build flag)
     */
    TFLiteModel(const char* model_name = "unknown",
                const char* model_type = "TFLite",
                uint16_t input_features = FEATURE_COUNT,
                uint16_t window_size = WINDOW_SIZE,
                uint16_t num_classes = NUM_CLASSES,
                uint32_t arena_size = TFLITE_MODEL_ARENA_SIZE)
        : _info(),
          _interpreter(nullptr),
          _tensor_arena(nullptr),
          _model(nullptr),
          _model_data(nullptr),
          _model_size(0),
          _input_buffer(nullptr),
          _output_buffer(nullptr),
          _input_size(0),
          _output_size(0),
          _ready(false),
          _last_inference_us(0),
          _arena_size(arena_size) {

        // Fill in model info
        strncpy(_info.name, model_name, sizeof(_info.name) - 1);
        strncpy(_info.type, model_type, sizeof(_info.type) - 1);
        _info.input_features = input_features;
        _info.window_size = window_size;
        _info.num_classes = num_classes;
        _info.model_size_bytes = 0;
        _info.arena_size_bytes = arena_size;
    }

    // =========================================================================
    // Destructor
    // =========================================================================

    ~TFLiteModel() override {
        cleanup();
    }

    // =========================================================================
    // BaseModel Interface Implementation
    // =========================================================================

    /**
     * @brief Initialize TFLite Micro interpreter with the given model data.
     *
     * Steps:
     *   1. Verify model flatbuffer
     *   2. Allocate tensor arena in PSRAM
     *   3. Create interpreter with all ops resolver
     *   4. Allocate tensors
     *   5. Verify input/output tensor shapes
     *   6. Initialize normalization statistics
     *
     * @param model_data  Pointer to TFLite flatbuffer (typically PROGMEM).
     * @param model_size  Size of the flatbuffer in bytes.
     * @return true if interpreter is ready for inference.
     */
    bool init(const uint8_t* model_data, size_t model_size) override {
        if (!model_data || model_size == 0) {
            Serial.println("[TFLiteModel] ERROR: Null model data");
            return false;
        }

        Serial.printf("[TFLiteModel] Initializing '%s' (%zu bytes)...\n",
                      _info.name, model_size);

        // Clean up any previous interpreter
        cleanup();

        _model_data = model_data;
        _model_size = model_size;

        // ---- Step 1: Load model from flatbuffer ----
        _model = tflite::GetModel(model_data);
        if (_model->version() != TFLITE_SCHEMA_VERSION) {
            Serial.printf("[TFLiteModel] ERROR: Model schema version %lu != expected %d\n",
                          (unsigned long)_model->version(), TFLITE_SCHEMA_VERSION);
            return false;
        }

        // ---- Step 2: Allocate tensor arena in PSRAM ----
        _tensor_arena = (uint8_t*)ps_malloc(_arena_size);
        if (!_tensor_arena) {
            Serial.printf("[TFLiteModel] ERROR: Failed to allocate %lu bytes in PSRAM\n",
                          (unsigned long)_arena_size);
            return false;
        }
        Serial.printf("[TFLiteModel] Arena: %lu bytes in PSRAM @ 0x%08X\n",
                      (unsigned long)_arena_size, (uint32_t)_tensor_arena);

        // ---- Step 3: Build interpreter ----
        static tflite::AllOpsResolver resolver;
        _interpreter = new tflite::MicroInterpreter(_model, resolver,
                                                     _tensor_arena, _arena_size,
                                                     &_error_reporter);

        // ---- Step 4: Allocate tensors ----
        TfLiteStatus alloc_status = _interpreter->AllocateTensors();
        if (alloc_status != kTfLiteOk) {
            Serial.println("[TFLiteModel] ERROR: AllocateTensors() failed");
            cleanup();
            return false;
        }

        // ---- Step 5: Verify tensor shapes ----
        _input_buffer = _interpreter->input(0)->data.f;
        _output_buffer = _interpreter->output(0)->data.f;
        _input_size = _interpreter->input(0)->bytes / sizeof(float);
        _output_size = _interpreter->output(0)->bytes / sizeof(float);

        size_t expected_input = (size_t)_info.window_size * _info.input_features;
        if (_input_size != (int)expected_input) {
            Serial.printf("[TFLiteModel] WARNING: Input size mismatch! "
                          "Model expects %d, config says %d\n",
                          _input_size, (int)expected_input);
            // Update info to match actual model
            _info.input_features = _input_size / _info.window_size;
        }

        if (_output_size != _info.num_classes) {
            Serial.printf("[TFLiteModel] WARNING: Output size mismatch! "
                          "Model outputs %d, config says %d\n",
                          _output_size, _info.num_classes);
            _info.num_classes = _output_size;
        }

        _info.model_size_bytes = model_size;

        // Log memory usage
        Serial.printf("[TFLiteModel] Input:  [%d] = %d frames × %d features\n",
                      _input_size, _info.window_size, _info.input_features);
        Serial.printf("[TFLiteModel] Output: [%d classes]\n", _output_size);
        Serial.printf("[TFLiteModel] Arena used: %lu / %lu bytes\n",
                      (unsigned long)_interpreter->arena_used_bytes(),
                      (unsigned long)_arena_size);

        _ready = true;
        Serial.printf("[TFLiteModel] '%s' ready for inference\n", _info.name);
        return true;
    }

    /**
     * @brief Release interpreter, arena, and all allocated resources.
     */
    void cleanup() override {
        _ready = false;
        if (_interpreter) {
            delete _interpreter;
            _interpreter = nullptr;
        }
        _input_buffer = nullptr;
        _output_buffer = nullptr;

        if (_tensor_arena) {
            free(_tensor_arena);
            _tensor_arena = nullptr;
        }

        _input_size = 0;
        _output_size = 0;
        _model = nullptr;
        Serial.println("[TFLiteModel] Cleaned up");
    }

    /**
     * @brief Preprocess sensor frames: normalize to zero mean, unit variance.
     *
     * For each feature, applies: normalized = (raw - mean) / (std + eps)
     *
     * Default normalization: mean=0, std=1 (identity transform).
     * Override setNormStats() to load training-set statistics.
     *
     * @param input       Output buffer (size ≥ window_size × input_features).
     * @param frames      Input sensor frames.
     * @param num_frames  Number of frames (should == window_size).
     * @return Total number of floats written, or -1 on error.
     */
    int preprocess(float* input, const SensorData* frames, int num_frames) override {
        if (!_ready || !input || !frames) return -1;

        // Take only the last `window_size` frames (or all if fewer)
        int start = std::max(0, num_frames - (int)_info.window_size);
        int actual_frames = num_frames - start;

        if (actual_frames <= 0) return -1;

        float features[FEATURE_COUNT];

        for (int f = 0; f < actual_frames; f++) {
            const SensorData& frame = frames[start + f];
            frame.toFeatureArray(features);

            for (int i = 0; i < (int)_info.input_features && i < FEATURE_COUNT; i++) {
                int idx = f * _info.input_features + i;
                input[idx] = normalize(features[i], i);
            }
        }

        // Zero-pad if we have fewer frames than window_size
        for (int f = actual_frames; f < (int)_info.window_size; f++) {
            for (int i = 0; i < (int)_info.input_features; i++) {
                int idx = f * _info.input_features + i;
                input[idx] = 0.0f;
            }
        }

        return (int)(_info.window_size * _info.input_features);
    }

    /**
     * @brief Run TFLite Micro inference.
     *
     * Copies preprocessed input into the interpreter's input tensor,
     * invokes the model, and copies the output tensor.
     *
     * @param input   Preprocessed input (size = input_size).
     * @param output  Output buffer (size ≥ num_classes).
     * @return Number of output values, or -1 on error.
     */
    int infer(const float* input, float* output) override {
        if (!_ready || !input || !output) return -1;

        // Copy input to interpreter tensor
        memcpy(_input_buffer, input, _input_size * sizeof(float));

        // Run inference
        uint32_t t0 = esp_timer_get_time();
        TfLiteStatus status = _interpreter->Invoke();
        uint32_t t1 = esp_timer_get_time();
        _last_inference_us = t1 - t0;

        if (status != kTfLiteOk) {
            Serial.printf("[TFLiteModel] Invoke() failed with status %d\n", status);
            return -1;
        }

        // Copy output
        memcpy(output, _output_buffer, _output_size * sizeof(float));
        return _output_size;
    }

    /**
     * @brief Post-process logits: softmax → argmax → confidence check.
     *
     * Applies in-place softmax to the output array, then finds the argmax.
     * If confidence < 0.3, marks result as invalid.
     * If confidence is in [0.3, 0.6), requests L2 inference.
     *
     * @param output  Raw logits (overwritten with probabilities).
     * @param result  Output GestureResult.
     * @return 0 on success.
     */
    int postprocess(const float* output, GestureResult* result) override {
        if (!output || !result) return -1;

        result->zero();

        // ---- Step 1: Softmax (in-place) ----
        float max_logit = output[0];
        for (int i = 1; i < _output_size; i++) {
            if (output[i] > max_logit) max_logit = output[i];
        }

        float sum_exp = 0.0f;
        float probs[NUM_CLASSES];  // Stack-allocated, max 46×4 = 184 bytes
        for (int i = 0; i < _output_size; i++) {
            probs[i] = expf(output[i] - max_logit);  // Numerically stable
            sum_exp += probs[i];
        }

        for (int i = 0; i < _output_size && i < NUM_CLASSES; i++) {
            probs[i] /= sum_exp;
            result->scores[i] = probs[i];
        }

        // ---- Step 2: Argmax ----
        int best_idx = 0;
        float best_prob = probs[0];
        for (int i = 1; i < _output_size; i++) {
            if (probs[i] > best_prob) {
                best_prob = probs[i];
                best_idx = i;
            }
        }

        result->gesture_id = best_idx;
        result->confidence = best_prob;

        // ---- Step 3: Confidence gating ----
        if (best_prob >= 0.6f) {
            result->valid = true;
            result->l2_requested = false;
        } else if (best_prob >= 0.3f) {
            // Uncertain — request L2 cloud/edge inference
            result->valid = false;
            result->l2_requested = true;
        } else {
            // Below threshold — no valid prediction
            result->valid = false;
            result->l2_requested = false;
        }

        return 0;
    }

    // =========================================================================
    // Metadata
    // =========================================================================

    ModelInfo get_model_info() const override { return _info; }

    const char* name() const override { return _info.name; }

    bool isReady() const override { return _ready; }

    uint32_t lastInferenceTimeUs() const override { return _last_inference_us; }

    // =========================================================================
    // Normalization Configuration
    // =========================================================================

    /**
     * @brief Set per-feature normalization statistics (from training set).
     *
     * @param means  Array of mean values (size = input_features).
     * @param stds   Array of std deviation values (size = input_features).
     * @param count  Number of features.
     */
    void setNormStats(const float* means, const float* stds, uint16_t count) {
        _norm_means = means;
        _norm_stds = stds;
        _norm_count = count;
        Serial.printf("[TFLiteModel] Normalization stats set for %d features\n", count);
    }

private:
    // =========================================================================
    // Members
    // =========================================================================

    ModelInfo               _info;
    tflite::MicroInterpreter* _interpreter;  ///< TFLite Micro interpreter
    tflite::MicroErrorReporter _error_reporter;  ///< TFLite error reporter
    uint8_t*                _tensor_arena;  ///< PSRAM tensor arena
    const tflite::Model*    _model;         ///< TFLite flatbuffer model
    const uint8_t*          _model_data;    ///< Raw model binary pointer
    size_t                  _model_size;    ///< Model binary size
    float*                  _input_buffer;  ///< Interpreter input tensor
    float*                  _output_buffer; ///< Interpreter output tensor
    int                     _input_size;    ///< Input tensor element count
    int                     _output_size;   ///< Output tensor element count
    bool                    _ready;         ///< Initialization flag
    uint32_t                _last_inference_us;  ///< Last inference latency
    uint32_t                _arena_size;    ///< Arena allocation size

    // Normalization
    const float*            _norm_means;    ///< Per-feature means (ptr, not owned)
    const float*            _norm_stds;     ///< Per-feature stds (ptr, not owned)
    uint16_t                _norm_count;    ///< Number of norm features

    // =========================================================================
    // Normalization Helper
    // =========================================================================

    /**
     * @brief Normalize a single feature value using z-score.
     * Falls back to identity (no normalization) if stats not set.
     */
    float normalize(float raw, int feature_idx) const {
        if (_norm_means && _norm_stds && feature_idx < _norm_count) {
            float std = _norm_stds[feature_idx];
            if (std < 1e-6f) std = 1e-6f;  // Prevent division by zero
            return (raw - _norm_means[feature_idx]) / std;
        }
        return raw;  // Identity if no normalization configured
    }
};

#endif // TFLITE_MODEL_H
