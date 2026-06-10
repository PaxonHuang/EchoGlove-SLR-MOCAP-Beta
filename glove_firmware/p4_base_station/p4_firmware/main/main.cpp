#include <cstdio>
#include <cmath>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "uart_receiver.h"
#include "data_structures.h"
#include "FramePairer.h"

// TFLite Micro inference (stub until model_data.h is generated)
#if __has_include("model_data.h")
#include "model_data.h"
#define HAS_MODEL 1
#else
#define HAS_MODEL 0
#endif
#include "tflite_infer.h"
#include "display_task.h"
#include "audio_task.h"
#include "usb_task.h"

static const char* TAG = "p4_main";

static FramePairer s_pairer;

// Queue for passing left[11] + right[11] feature pairs to the inference task
struct FeaturePair {
    float left[TFLITE_INPUT_DIM];
    float right[TFLITE_INPUT_DIM];
};

static QueueHandle_t s_inference_queue = nullptr;

// Latest data for display updates (written by inference/uart tasks)
static Tier2Result     s_last_result = {};
static FramePair       s_last_pair   = {};
static float           s_last_features[DUAL_HAND_FEATURES] = {};
static BaseStationStatus s_last_status = {};
static volatile bool   s_has_new_result = false;

// Compute relative features from GlovePacket imu[6] (euler[3] + gyro[3]).
// Cannot use RelativeFeatures.compute() -- it requires quaternion[4]
// which is NOT transmitted in GlovePacket.imu[6].
static void compute_relative_from_imu(
    const float* left_imu, const float* right_imu, float* out) {
    memset(out, 0, 6 * sizeof(float));
    // out[0..2] = delta euler (left - right)
    for (int i = 0; i < 3; i++)
        out[i] = left_imu[i] - right_imu[i];
    // out[3] = euler distance (L2 norm of delta euler)
    float dist_sq = 0;
    for (int i = 0; i < 3; i++) dist_sq += out[i] * out[i];
    out[3] = sqrtf(dist_sq);
    // out[4] = delta gyro norm
    float norm_l = 0, norm_r = 0;
    for (int i = 3; i < 6; i++) {
        norm_l += left_imu[i] * left_imu[i];
        norm_r += right_imu[i] * right_imu[i];
    }
    out[4] = sqrtf(norm_l) - sqrtf(norm_r);
    // out[5] = max gyro axis difference (normalized index)
    float max_diff = 0; int max_idx = 0;
    for (int i = 3; i < 6; i++) {
        float diff = fabsf(left_imu[i] - right_imu[i]);
        if (diff > max_diff) { max_diff = diff; max_idx = i - 3; }
    }
    out[5] = max_idx / 3.0f;
}

// ---- Inference task (pinned to core 1) ----

static void inference_task(void* arg) {
    FeaturePair fp;
    uint32_t infer_count = 0;

    while (1) {
        if (xQueueReceive(s_inference_queue, &fp, pdMS_TO_TICKS(100)) == pdTRUE) {
            Tier2Result result = tflite_run(fp.left, fp.right);
            infer_count++;

            if (infer_count % 10 == 0) {
                ESP_LOGI(TAG, "Tier2 #%lu: gesture=%d conf=%.3f time=%lu us",
                         (unsigned long)infer_count,
                         result.gesture_id,
                         result.confidence,
                         (unsigned long)result.inference_us);
            }

            // Store latest result for display
            s_last_result = result;
            s_has_new_result = true;

            // Audio: play TTS if confident prediction
            if (result.valid && result.confidence > 0.5f) {
                audio_play_gesture(result.gesture_id);
            }

            // USB CDC: send result JSON
            usb_task_send_result(&result, nullptr, nullptr);
        }
    }
}

// ---- UART receive task (pinned to core 0) ----

static void uart_task(void* arg) {
    GlovePacket pkt;
    while (1) {
        if (uart_receiver_poll(&pkt)) {
            s_pairer.feed(pkt);
            FramePair pair;
            if (s_pairer.getPair(pair)) {
                float relative[6];
                compute_relative_from_imu(pair.left.imu, pair.right.imu, relative);

                float features[DUAL_HAND_FEATURES]; // 28
                memcpy(features, pair.left.flex, 5 * sizeof(float));
                memcpy(features + 5, pair.left.imu, 6 * sizeof(float));
                memcpy(features + 11, pair.right.flex, 5 * sizeof(float));
                memcpy(features + 16, pair.right.imu, 6 * sizeof(float));
                memcpy(features + 22, relative, 6 * sizeof(float));

                ESP_LOGI(TAG, "Pair tick=%lu feat[0]=%.3f feat[27]=%.3f",
                         (unsigned long)pair.tick_id, features[0], features[27]);

                // Store latest pair + features for display
                s_last_pair = pair;
                memcpy(s_last_features, features, sizeof(s_last_features));

                // Send left[11] and right[11] to inference task
                // Tier2 model uses only flex+imu per hand, not relative features
                FeaturePair fp;
                memcpy(fp.left, pair.left.flex, 5 * sizeof(float));
                memcpy(fp.left + 5, pair.left.imu, 6 * sizeof(float));
                memcpy(fp.right, pair.right.flex, 5 * sizeof(float));
                memcpy(fp.right + 5, pair.right.imu, 6 * sizeof(float));

                // Non-blocking enqueue; drop frame if queue is full
                xQueueSend(s_inference_queue, &fp, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "P4 base station starting");

    // Initialize TFLite Micro inference
#if HAS_MODEL
    bool tfl_ok = tflite_init(model_data, model_data_len);
    ESP_LOGI(TAG, "TFLite init: %s (%u bytes arena)",
             tfl_ok ? "OK" : "FAIL", (unsigned)tflite_arena_usage());
#else
    ESP_LOGW(TAG, "model_data.h not found -- Tier2 inference disabled");
    // Still init the stub so tflite_run() doesn't warn every call
    tflite_init(nullptr, 0);
#endif

    // Create inference queue (capacity 4 frames — small to back-pressure)
    s_inference_queue = xQueueCreate(4, sizeof(FeaturePair));
    assert(s_inference_queue != nullptr);

    // Initialize audio, USB, and display subsystems
    audio_init();
    usb_task_init();
    display_init();

    // Build initial status
    s_last_status.p4_ready = true;
    s_last_status.active_tier = 2;

    // Start UART receiver on core 0, inference on core 1
    uart_receiver_init(0, 37, 38, 2000000);
    xTaskCreatePinnedToCore(uart_task, "uart_rx", 8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(inference_task, "inference", 8192, NULL, 2, NULL, 1);

    ESP_LOGI(TAG, "P4 ready -- UART + inference + display running");

    while (1) {
        // Update display when new inference result is available
        if (s_has_new_result) {
            display_update(&s_last_result, &s_last_pair,
                           s_last_features, &s_last_status);
            s_has_new_result = false;
        }

        ESP_LOGI(TAG, "UART RX count: %lu", (unsigned long)uart_receiver_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
