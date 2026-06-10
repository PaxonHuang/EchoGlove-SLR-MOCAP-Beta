/* =============================================================================
 * EchoGlove V5.2 — ESP-NOW Receive Handler Implementation
 * =============================================================================
 * Pure ESP-IDF (no Arduino). Validates incoming GlovePackets against magic,
 * version, and CRC-16/MODBUS before forwarding via callback.
 * =============================================================================
 */

#include "espnow_handler.h"
#include <cstring>
#include "esp_now.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char* TAG = "espnow_rx";
static espnow_packet_cb_t s_callback = nullptr;
static uint32_t s_rx_count = 0;

static void on_espnow_recv(const esp_now_recv_info_t* info,
                           const uint8_t* data, int len) {
    if (len != sizeof(GlovePacket)) {
        ESP_LOGD(TAG, "Bad len: %d (expected %d)", len, (int)sizeof(GlovePacket));
        return;
    }
    GlovePacket pkt;
    memcpy(&pkt, data, sizeof(GlovePacket));

    // Validate magic bytes "EG"
    if (pkt.magic[0] != 0x45 || pkt.magic[1] != 0x47) return;
    // Validate protocol version
    if (pkt.version != 5) return;
    // Validate CRC-16/MODBUS checksum
    if (!pkt.verifyChecksum()) {
        ESP_LOGW(TAG, "CRC fail");
        return;
    }

    s_rx_count++;
    if (s_callback) {
        s_callback(&pkt);
    }
}

extern "C" bool espnow_handler_init(espnow_packet_cb_t cb) {
    s_callback = cb;

    // Init NVS (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init network + WiFi STA (required for ESP-NOW)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW and register receive callback
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_espnow_recv));

    ESP_LOGI(TAG, "ESP-NOW receiver initialized");
    return true;
}

extern "C" uint32_t espnow_handler_rx_count(void) {
    return s_rx_count;
}
