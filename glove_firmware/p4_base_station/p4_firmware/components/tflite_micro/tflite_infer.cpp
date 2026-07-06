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

    if (!s_initialized) {
        ESP_LOGW(TAG, "tflite_run called before tflite_init");
        result.gesture_id = -1;
        result.valid = false;
        return result;
    }

    int64_t t0 = esp_timer_get_time();

#if __has_include("model_data.h")
    // ---- Real inference path (model_data.h exists) ----
    // TODO: full TFLite Micro interpreter pipeline
    //   1. Copy left[11] → input tensor 0
    //   2. Copy right[11] → input tensor 1
    //   3. Invoke
    //   4. Parse output → softmax → argmax
    // For now, fall through to stub behavior until interpreter is wired.
#endif

    // ---- Stub: cycle through 5 gestures with 0.80 confidence ----
    // Enables LVGL display + USB CDC demo before model_data.h exists.
    static int s_call_count = 0;
    const int STUB_GESTURES[5] = {0, 1, 2, 3, 4};  // 你好, 谢谢, 对不起, 是, 不是

    result.gesture_id = STUB_GESTURES[s_call_count % 5];
    result.confidence = 0.80f;
    result.valid = true;

    s_call_count++;

    int64_t t1 = esp_timer_get_time();
    result.inference_us = (uint32_t)(t1 - t0);

    ESP_LOGD(TAG, "STUB inference: gesture=%d conf=%.2f call=%d",
             result.gesture_id, result.confidence, s_call_count);

    return result;
}

uint32_t tflite_arena_usage(void) {
    // TODO: return s_interpreter->arena_used_bytes();
    return 0;
}
