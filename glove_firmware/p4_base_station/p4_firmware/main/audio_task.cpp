#include "audio_task.h"
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "p4_protocol.h"

static const char* TAG = "audio";
static int64_t s_last_play_time[46] = {};
static const int64_t DEDUP_US = P4_TTS_DEDUP_MS * 1000; // convert ms to us

bool audio_init(void) {
    ESP_LOGI(TAG, "Audio init (ES8311 I2S)");
    // TODO: Initialize esp_codec_dev with ES8311 codec
    // Use BSP audio init from esp-bsp
    return true;
}

void audio_play_gesture(int gesture_id) {
    if (gesture_id < 0 || gesture_id >= 46) return;

    int64_t now = esp_timer_get_time();
    if (now - s_last_play_time[gesture_id] < DEDUP_US) {
        return; // Dedup
    }
    s_last_play_time[gesture_id] = now;

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/tts/%02d.pcm", gesture_id);
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "PCM not found: %s", path);
        return;
    }

    uint8_t buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        // TODO: i2s_write(I2S_NUM_0, buf, bytes_read, &bytes_written, portMAX_DELAY);
    }
    fclose(f);
    ESP_LOGI(TAG, "Played gesture %d", gesture_id);
}
