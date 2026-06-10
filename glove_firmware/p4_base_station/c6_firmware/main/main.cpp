/* =============================================================================
 * EchoGlove V5.2 — C6 Co-Processor Main
 * =============================================================================
 * ESP32-C6 firmware for the P4 base station co-processor.
 * Receives GlovePackets from gloves via ESP-NOW, wraps them in UART frames
 * (magic + payload + CRC16), and relays them over UART to the ESP32-P4.
 *
 * UART: 2 Mbps on GPIO 43 (TX) / GPIO 44 (RX)
 * ESP-NOW: receives 69-byte GlovePacket from glove ESP32-S3 devices
 * =============================================================================
 */

#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "espnow_handler.h"
#include "data_structures.h"
#include "uart_frame.h"

static const char* TAG = "c6_main";

#define UART_PORT      UART_NUM_0
#define UART_TX_PIN    43
#define UART_RX_PIN    44
#define UART_BAUD      2000000
#define UART_BUF_SIZE  1024

static QueueHandle_t s_uart_queue = nullptr;

// ESP-NOW callback: wrap packet in UART frame and enqueue for TX
static void on_glove_packet(const GlovePacket* pkt) {
    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t frame_len = uart_frame_encode(pkt, sizeof(GlovePacket),
                                          frame, sizeof(frame));
    if (frame_len > 0 && s_uart_queue) {
        // Enqueue frame bytes for UART transmission
        for (size_t i = 0; i < frame_len; i++) {
            uint8_t byte = frame[i];
            xQueueSend(s_uart_queue, &byte, 0);
        }
    }
}

static void uart_tx_task(void* arg) {
    uint8_t byte;
    uint8_t tx_buf[UART_FRAME_MAX_SIZE];
    size_t tx_idx = 0;

    while (1) {
        if (xQueueReceive(s_uart_queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE) {
            tx_buf[tx_idx++] = byte;
            // Send complete frames immediately (73 bytes)
            if (tx_idx >= UART_FRAME_MAX_SIZE) {
                uart_write_bytes(UART_PORT, tx_buf, tx_idx);
                tx_idx = 0;
            }
        } else if (tx_idx > 0) {
            // Flush partial buffer on timeout (should not happen for complete frames)
            uart_write_bytes(UART_PORT, tx_buf, tx_idx);
            tx_idx = 0;
        }
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "C6 base station starting");

    // Configure UART at 2 Mbps
    uart_config_t uart_cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Create UART TX queue (enough for several frames)
    s_uart_queue = xQueueCreate(UART_FRAME_MAX_SIZE * 4, sizeof(uint8_t));

    // Init ESP-NOW receiver with packet callback
    espnow_handler_init(on_glove_packet);

    // Start UART TX task on core 0
    xTaskCreate(uart_tx_task, "uart_tx", 4096, nullptr, 3, nullptr);

    ESP_LOGI(TAG, "C6 ready — ESP-NOW → UART relay active");

    // Status log loop
    while (1) {
        ESP_LOGI(TAG, "RX count: %lu", (unsigned long)espnow_handler_rx_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
