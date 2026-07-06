#include "usb_task.h"
#include <cstring>
#include <cstdio>
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tinyusb_types.h"

static const char* TAG = "usb";
static bool s_usb_ready = false;

// CDC RX callback (optional — for future PC→P4 commands)
static void cdc_rx_callback(int itf, cdcacm_event_t *event) {
    (void)itf;
    (void)event;
    ESP_LOGD(TAG, "CDC RX on itf=%d", itf);
}

bool usb_task_init(void) {
    ESP_LOGI(TAG, "USB HS CDC init (TinyUSB)");

    // 1. Install TinyUSB core driver (descriptors come from Kconfig defaults)
    tinyusb_config_t tusb_cfg = {
        .device_descriptor  = NULL,
        .string_descriptor  = NULL,
        .external_phy       = false,
#if (TUD_OPT_HIGH_SPEED)
        .hs_configuration_descriptor = NULL,
#endif
    };
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 2. Init CDC ACM on port 0 with RX callback
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev                = TINYUSB_USBDEV_0,
        .cdc_port               = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz       = 0,
        .callback_rx            = cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    ret = tusb_cdc_acm_init(&acm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CDC ACM init failed: %s", esp_err_to_name(ret));
        return false;
    }

    s_usb_ready = true;
    ESP_LOGI(TAG, "USB CDC initialized — P4 will send JSON inference results");
    return true;
}

void usb_task_send_result(const Tier2Result* result,
                           const FramePair* pair,
                           const float features[DUAL_HAND_FEATURES]) {
    if (!result) return;
    if (!s_usb_ready) {
        ESP_LOGD(TAG, "USB not ready, skipping TX");
        return;
    }

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"gesture\":%d,\"conf\":%.3f,\"tier2_time\":%lu}\n",
        result->gesture_id, result->confidence,
        (unsigned long)result->inference_us);

    size_t written = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                                (const uint8_t*)buf, (size_t)n);
    // Non-blocking flush (timeout=0): let the TinyUSB task drain the queue
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);

    if (written != (size_t)n) {
        ESP_LOGW(TAG, "USB partial write: %u/%d bytes", (unsigned)written, n);
    }
}
