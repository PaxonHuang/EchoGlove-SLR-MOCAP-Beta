/* =============================================================================
 * EchoGlove V5.2 — P4 Internal Mock Data Source Implementation
 * =============================================================================
 * See mock_data_source.h for design notes. This module is only compiled when
 * CONFIG_P4_INTERNAL_MOCK=y; main.cpp conditionally calls it instead of
 * uart_receiver_*.
 * =============================================================================
 */

#include "mock_data_source.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "p4_mock";

static QueueHandle_t s_queue = nullptr;
static uint32_t s_count = 0;

// 5 distinct flex patterns (open, fist, thumbs_up, peace, point).
// Identical to c6_firmware espnow_handler.cpp MOCK_FLEX so the two mocks
// produce the same gesture cycle.
static const int MOCK_GESTURES = 5;
static const float MOCK_FLEX[5][5] = {
    {0.1f, 0.1f, 0.1f, 0.1f, 0.1f},  // open_hand
    {0.9f, 0.9f, 0.9f, 0.9f, 0.9f},  // fist
    {0.1f, 0.9f, 0.1f, 0.1f, 0.1f},  // thumbs_up
    {0.1f, 0.1f, 0.9f, 0.9f, 0.1f},  // peace
    {0.1f, 0.9f, 0.1f, 0.1f, 0.1f},  // point (index)
};

static void mock_task(void* arg) {
    int gesture_idx = 0;
    bool toggle_left = true;  // alternate L/R to produce pairs
    uint32_t tick = 0;

    ESP_LOGI(TAG, "P4 internal mock task started — 5 gestures @ 50Hz/hand");

    while (1) {
        GlovePacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;  // "EG"
        pkt.version = 5;
        pkt.hand_id = toggle_left ? HAND_LEFT : HAND_RIGHT;
        pkt.tick_id = tick;
        pkt.timestamp_us = (uint32_t)esp_timer_get_time();  // real wall-clock for FramePairer timeout
        // Flex: copy pattern for current gesture
        memcpy(pkt.flex, MOCK_FLEX[gesture_idx % MOCK_GESTURES], sizeof(float) * 5);
        // IMU: small synthetic values (euler[3] + gyro[3])
        for (int i = 0; i < 6; i++) pkt.imu[i] = 0.1f * (i + 1);
        pkt.computeChecksum();

        if (s_queue) {
            // Non-blocking enqueue; drop if queue full (uart_task polls fast)
            xQueueSend(s_queue, &pkt, 0);
        }

        if (!toggle_left) {
            tick++;           // pair complete after R
            gesture_idx++;    // advance gesture every full pair
        }
        toggle_left = !toggle_left;

        // 50Hz per hand → 20ms; alternating L/R means 10ms per packet
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool mock_data_source_init(void) {
    s_queue = xQueueCreate(8, sizeof(GlovePacket));
    if (!s_queue) {
        ESP_LOGE(TAG, "Failed to create mock queue");
        return false;
    }
    ESP_LOGW(TAG, "P4 INTERNAL MOCK mode — generating synthetic data (no UART/C6)");
    xTaskCreate(mock_task, "p4_mock", 4096, nullptr, 4, nullptr);
    return true;
}

bool mock_data_source_poll(GlovePacket* out) {
    if (!s_queue || !out) return false;
    if (xQueueReceive(s_queue, out, 0) == pdTRUE) {
        s_count++;
        return true;
    }
    return false;
}

uint32_t mock_data_source_count(void) {
    return s_count;
}
