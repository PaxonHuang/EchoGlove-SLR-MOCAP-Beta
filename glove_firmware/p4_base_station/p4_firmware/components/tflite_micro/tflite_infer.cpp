/**
 * @file tflite_infer.cpp
 * @brief TFLite Micro inference stubs for Tier2 GatedBiCrossAttention.
 *
 * This file provides the C-linkage API defined in tflite_infer.h.
 * The actual TFLite Micro interpreter calls are TODO stubs — they need
 * the TFLite Micro library headers (via ESP-DL or a vendored build)
 * to be fully wired up.
 *
 * Integration checklist:
 *   1. Add tflite-micro as a component or lib_deps
 *   2. Include tensorflow/lite/micro/micro_interpreter.h
 *   3. Allocate arena in PSRAM (esp_psram)
 *   4. Wire input/output tensors for two 11-dim inputs
 */

#include "tflite_infer.h"

#include <cmath>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "tflite";

// ---- Internal state (stub) ----

static bool s_initialized = false;

// TODO: Add these when TFLite Micro is integrated:
// static const tflite::Model* s_model = nullptr;
// static tflite::MicroInterpreter* s_interpreter = nullptr;
// static constexpr int kArenaSize = 64 * 1024;  // adjust per model

// ---- Public API ----

bool tflite_init(const unsigned char* model_data, unsigned int model_len) {
    ESP_LOGI(TAG, "TFLite init (stub) -- model size: %u bytes", model_len);

    // TODO: Full TFLite Micro init
    //   1. s_model = tflite::GetModel(model_data);
    //   2. static tflite::MicroMutableOpResolver<16> resolver;
    //      resolver.AddFullyConnected();
    //      resolver.AddMul();
    //      resolver.AddAdd();
    //      resolver.AddLogistic();     // sigmoid
    //      resolver.AddSoftmax();
    //      resolver.AddReshape();
    //      resolver.AddConcatenation();
    //   3. uint8_t* arena = (uint8_t*)heap_caps_malloc(kArenaSize, MALLOC_CAP_SPIRAM);
    //   4. s_interpreter = new tflite::MicroInterpreter(s_model, resolver, arena, kArenaSize);
    //   5. s_interpreter->AllocateTensors();

    s_initialized = true;
    ESP_LOGI(TAG, "TFLite init OK (stub)");
    return true;
}

Tier2Result tflite_run(const float left[TFLITE_INPUT_DIM],
                       const float right[TFLITE_INPUT_DIM]) {
    Tier2Result result;
    memset(&result, 0, sizeof(result));
    result.gesture_id = -1;
    result.valid = false;

    if (!s_initialized) {
        ESP_LOGW(TAG, "tflite_run called before tflite_init");
        return result;
    }

    int64_t t0 = esp_timer_get_time();

    // TODO: Full inference pipeline
    //   1. Copy left[11] → input tensor 0
    //      float* in_left = s_interpreter->input(0)->data.f;
    //      memcpy(in_left, left, TFLITE_INPUT_DIM * sizeof(float));
    //   2. Copy right[11] → input tensor 1
    //      float* in_right = s_interpreter->input(1)->data.f;
    //      memcpy(in_right, right, TFLITE_INPUT_DIM * sizeof(float));
    //   3. Invoke
    //      TfLiteStatus status = s_interpreter->Invoke();
    //   4. Parse output tensor → softmax → argmax
    //      const float* out = s_interpreter->output(0)->data.f;
    //      ... compute softmax probabilities ...
    //      ... find argmax ...

    int64_t t1 = esp_timer_get_time();
    result.inference_us = (uint32_t)(t1 - t0);
    result.valid = true;

    return result;
}

uint32_t tflite_arena_usage(void) {
    // TODO: return s_interpreter->arena_used_bytes();
    return 0;
}
