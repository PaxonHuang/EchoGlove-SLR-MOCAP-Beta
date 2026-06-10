#include "usb_task.h"
#include <cstring>
#include <cstdio>
#include "esp_log.h"

static const char* TAG = "usb";

bool usb_task_init(void) {
    ESP_LOGI(TAG, "USB HS CDC init");
    // TODO: TinyUSB CDC ACM initialization
    return true;
}

void usb_task_send_result(const Tier2Result* result,
                           const FramePair* pair,
                           const float features[DUAL_HAND_FEATURES]) {
    if (!result) return;

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"gesture\":%d,\"conf\":%.3f,\"tier2_time\":%lu}\n",
        result->gesture_id, result->confidence,
        (unsigned long)result->inference_us);

    // TODO: tud_cdc_n_write(0, (const uint8_t*)buf, n);
    // TODO: tud_cdc_n_write_flush(0);
    ESP_LOGD(TAG, "USB TX: %d bytes", n);
}
