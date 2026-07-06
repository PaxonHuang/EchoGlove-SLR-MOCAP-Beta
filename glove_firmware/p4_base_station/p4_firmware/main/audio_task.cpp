#include "audio_task.h"
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "p4_protocol.h"

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_types.h"

static const char* TAG = "audio";
static int64_t s_last_play_time[46] = {};
static const int64_t DEDUP_US = P4_TTS_DEDUP_MS * 1000; // convert ms to us

static esp_codec_dev_handle_t s_codec_dev = NULL;
static bool s_audio_ready = false;

bool audio_init(void) {
    ESP_LOGI(TAG, "Audio init (ES8311 via BSP)");

    /* Mount SD card so /sdcard/tts/ PCM files are reachable. Continue on failure
     * — audio_play_gesture will just log "PCM not found" without the card. */
    esp_err_t ret = bsp_sdcard_mount();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed: %s — TTS PCM files unavailable",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SD card mounted at /sdcard");
    }

    /* Initialize ES8311 codec via BSP (sets up I2S + ES8311 config). */
    s_codec_dev = bsp_audio_codec_speaker_init();
    if (!s_codec_dev) {
        ESP_LOGE(TAG, "ES8311 codec speaker init failed");
        return false;
    }

    /* Open output device: 16 kHz, 16-bit, mono — matches TTS PCM format. */
    esp_codec_dev_sample_info_t info = {};
    info.bits_per_sample = 16;
    info.channel         = 1;
    info.sample_rate     = 16000;

    int err = esp_codec_dev_open(s_codec_dev, &info);
    if (err != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Codec open failed: %d", err);
        return false;
    }

    s_audio_ready = true;
    ESP_LOGI(TAG, "Audio ready — ES8311 @ 16kHz/16bit/mono");
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
        if (s_audio_ready) {
            int written = esp_codec_dev_write(s_codec_dev, buf, (int)bytes_read);
            if (written < 0) {
                ESP_LOGW(TAG, "Codec write error: %d", written);
                break;
            }
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "Played gesture %d", gesture_id);
}
