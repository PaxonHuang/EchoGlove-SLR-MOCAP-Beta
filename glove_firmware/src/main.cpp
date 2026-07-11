/* =============================================================================
 * EchoGlove V6 — Main Entry Point (DualGloveFlex + Internal ADC1)
 * =============================================================================
 * FreeRTOS dual-core task architecture for ESP32-S3:
 *
 *   Core 1 (Protocol CPU):
 *     └─ Task_SensorRead  (Priority 3, 100 Hz)
 *        - Reads sensors via SensorManager (simulation or hardware)
 *        - V6: flex from internal ADC1 (GPIO1-5), IMU zeros (LSM6DSV16X pending)
 *        - Fills GlovePacket and sends to comms queue
 *
 *   Core 0 (Application CPU):
 *     └─ Task_Comms       (Priority 1, 50 Hz)
 *        - Reads from comms queue
 *        - Sends GlovePacket via ESP-NOW broadcast
 *        - Prints summary to Serial every 1s
 *
 * Per-glove config: set MY_HAND (HAND_LEFT / HAND_RIGHT) before flashing
 * each glove. Set SIMULATION=false for real hardware (V6 internal ADC1).
 *
 * Build: pio run
 * Upload: pio run -t upload
 * Monitor: pio device monitor -b 115200
 * =============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "data_structures.h"
#include "Sensors/SensorManager.h"

// ── Configuration ───────────────────────────────────────────────
static constexpr HandID    MY_HAND      = HAND_LEFT;   // Change per glove
static constexpr uint32_t  SENSOR_HZ    = 100;
static constexpr uint32_t  COMMS_HZ     = 50;
static constexpr bool      SIMULATION   = false;        // V6: real ADC1 hardware

// ── Broadcast address (send to all) ────────────────────────────
static const uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── Globals ────────────────────────────────────────────────────
static SensorManager g_sensors;
static QueueHandle_t g_comms_queue = nullptr;
static uint32_t g_packets_sent = 0;
static uint32_t g_last_print_ms = 0;

// ── ESP-NOW send callback ──────────────────────────────────────
static void on_send_done(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.printf("[ESPNOW] Send FAILED to %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac_addr[0], mac_addr[1], mac_addr[2],
                      mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

// ── Task: Sensor Read (Core 1, 100 Hz) ─────────────────────────
static void Task_SensorRead(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000 / SENSOR_HZ);

    uint32_t tick_id = 0;

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        // Read sensor data
        SensorData sd = g_sensors.read();

        // Fill GlovePacket
        GlovePacket pkt = {};
        pkt.magic[0] = 0x45;  // 'E'
        pkt.magic[1] = 0x47;  // 'G'
        pkt.version = 5;
        pkt.hand_id = static_cast<uint8_t>(MY_HAND);
        pkt.tick_id = tick_id++;
        pkt.timestamp_us = sd.timestamp_us;
        memcpy(pkt.flex, sd.flex, sizeof(pkt.flex));
        pkt.imu[0] = sd.euler[0];
        pkt.imu[1] = sd.euler[1];
        pkt.imu[2] = sd.euler[2];
        pkt.imu[3] = sd.gyro[0];
        pkt.imu[4] = sd.gyro[1];
        pkt.imu[5] = sd.gyro[2];
        pkt.l1_gesture_id = 0;
        pkt.l1_confidence = 0.0f;
        pkt.status = static_cast<uint8_t>(STATUS_STREAMING);
        pkt.computeChecksum();

        // ── DEMO: USB CDC ASCII flex stream for demo_server.py ──────────────
        // Every 3rd frame (~33 Hz): emit "$EG,f0,f1,f2,f3,f4,tick\n" over USB
        // CDC (which IS Serial here, ARDUINO_USB_CDC_ON_BOOT=1). flex values
        // are normalized 0=straight .. 1=bent. Parsed by the standalone
        // glove_relay/scripts/demo_server.py → rule classifier → V5 WS JSON.
        // ESP-NOW broadcast above is unaffected. ~8 lines, opt-out via flag.
        if (pkt.tick_id % 3 == 0) {
            Serial.printf("$EG,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n",
                          pkt.flex[0], pkt.flex[1], pkt.flex[2],
                          pkt.flex[3], pkt.flex[4], pkt.tick_id);
        }

        // Send to comms queue (non-blocking, drop if full)
        xQueueSend(g_comms_queue, &pkt, 0);
    }
}

// ── Task: Comms (Core 0, 50 Hz) ────────────────────────────────
static void Task_Comms(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000 / COMMS_HZ);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        GlovePacket pkt;
        if (xQueueReceive(g_comms_queue, &pkt, 0) == pdTRUE) {
            // Send via ESP-NOW
            esp_err_t err = esp_now_send(BROADCAST_ADDR,
                                          reinterpret_cast<const uint8_t*>(&pkt),
                                          sizeof(GlovePacket));
            if (err == ESP_OK) {
                g_packets_sent++;
            }
        }

        // Print status every 1 second
        uint32_t now = millis();
        if (now - g_last_print_ms >= 1000) {
            g_last_print_ms = now;
            Serial.printf("[V5] hand=%s seq=%u sent=%u gesture=%s sim=%d\n",
                          MY_HAND == HAND_LEFT ? "LEFT" : "RIGHT",
                          pkt.tick_id, g_packets_sent,
                          g_sensors.gestureName(g_sensors.currentGesture()),
                          g_sensors.isSimulation() ? 1 : 0);
        }
    }
}

// ── Setup ──────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  EchoGlove V5 — DualGloveFlex");
    Serial.printf("  Hand: %s\n", MY_HAND == HAND_LEFT ? "LEFT" : "RIGHT");
    Serial.printf("  Mode: %s\n", SIMULATION ? "SIMULATION" : "HARDWARE");
    Serial.println("========================================");

    // Initialize WiFi (required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] FATAL: init failed!");
        ESP.restart();
    }
    esp_now_register_send_cb(on_send_done);

    // Add broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 1;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESPNOW] WARNING: add broadcast peer failed");
    }
    Serial.println("[ESPNOW] Initialized, broadcast peer added");

    // Initialize sensor manager
    g_sensors.begin(SIMULATION);
    Serial.printf("[Sensors] Ready (mode=%s)\n",
                  g_sensors.isSimulation() ? "SIM" : "HW");

    // Create comms queue
    g_comms_queue = xQueueCreate(16, sizeof(GlovePacket));
    if (!g_comms_queue) {
        Serial.println("[RTOS] FATAL: queue create failed!");
        ESP.restart();
    }

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(
        Task_SensorRead, "SensorRead", 4096, nullptr, 3, nullptr, 1);

    xTaskCreatePinnedToCore(
        Task_Comms, "Comms", 4096, nullptr, 1, nullptr, 0);

    Serial.println("[RTOS] Tasks created: SensorRead@Core1, Comms@Core0");
    Serial.println("[V5] Boot complete — streaming data...");
}

void loop() {
    // Main loop is empty — all work is in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
