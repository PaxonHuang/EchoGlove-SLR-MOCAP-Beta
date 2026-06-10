#include <cstdio>
#include <cmath>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "uart_receiver.h"
#include "data_structures.h"
#include "FramePairer.h"

static const char* TAG = "p4_main";

static FramePairer s_pairer;

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
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "P4 base station starting");
    uart_receiver_init(0, 37, 38, 2000000);
    xTaskCreatePinnedToCore(uart_task, "uart_rx", 8192, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "P4 ready -- UART receiver active");

    while (1) {
        ESP_LOGI(TAG, "UART RX count: %lu", (unsigned long)uart_receiver_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
