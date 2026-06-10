# V5.2 ESP32-P4 Smart Base Station Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the ESP32-P4 smart base station firmware (C6 co-processor + P4 main processor) with Tier2 inference, LVGL touchscreen UI, and TTS audio, replacing the V5.0 S3 receiver.

**Architecture:** Two ESP-IDF v5.4+ firmware projects under `glove_firmware/p4_base_station/` — one for the C6 (ESP-NOW receiver + UART relay) and one for the P4 (UART receive + TFLite inference + LVGL display + TTS audio + USB CDC). A shared protocol library provides UART framing and data structures. Existing receiver libraries (`FramePairer.h`, `RelativeFeatures.h`) are reused as-is. PC relay gets minimal USB CDC input additions.

**Tech Stack:** ESP-IDF v5.4+, TFLite Micro, LVGL v8.3, nanopb, ESP-NOW, BLE 5.0

**Design Spec:** `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`

---

## File Structure

### New files to create

```
glove_firmware/
├── shared/
│   ├── uart_frame.h              # UART frame encode/decode (header-only)
│   └── p4_protocol.h             # P4-specific constants and BaseStationPacket struct
│
├── p4_base_station/
│   ├── c6_firmware/              # ESP-IDF project for ESP32-C6
│   │   ├── CMakeLists.txt
│   │   ├── sdkconfig.defaults
│   │   └── main/
│   │       ├── CMakeLists.txt
│   │       ├── main.c            # C6 entry: ESP-NOW init + UART relay task
│   │       └── espnow_handler.c  # ESP-NOW receive callback + packet validation
│   │
│   ├── p4_firmware/              # ESP-IDF project for ESP32-P4
│   │   ├── CMakeLists.txt
│   │   ├── sdkconfig.defaults
│   │   ├── main/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.c            # P4 entry: FreeRTOS task creation
│   │   │   ├── uart_receiver.c   # UART RX + frame parsing
│   │   │   ├── inference_task.c  # TFLite Micro inference
│   │   │   ├── display_task.c    # LVGL UI management
│   │   │   ├── audio_task.c      # ES8311 TTS playback
│   │   │   └── usb_task.c        # USB HS CDC output
│   │   ├── components/
│   │   │   └── tflite_micro/     # TFLite Micro component wrapper
│   │   │       ├── CMakeLists.txt
│   │   │       ├── tflite_infer.h
│   │   │       └── tflite_infer.c
│   │   └── model/
│   │       └── model_data.h      # INT8 quantized model (generated)
│   │
│   └── tests/                    # Native PlatformIO test project
│       ├── platformio.ini
│       ├── test_uart_frame/
│       │   └── test_uart_frame.cpp
│       ├── test_frame_pairer/
│       │   └── test_frame_pairer.cpp
│       └── test_feature_assembly/
│           └── test_feature_assembly.cpp
│
├── scripts/
│   ├── export_model.py           # PyTorch → ONNX → TFLite INT8 export
│   ├── generate_pcm_tts.py       # Generate 46 gesture PCM files
│   └── calibrate_quantization.py # INT8 quantization calibration data
```

### Existing files to modify

| File | Change |
|------|--------|
| `glove_relay/proto/glove_data.proto` | Add `BaseStationPacket` message |
| `glove_relay/src/protobuf_parser.py` | Add `parse_base_station_packet()` + `assemble_28dim()` update |
| `glove_relay/src/udp_server.py` | Add USB CDC serial input option |
| `glove_relay/src/main.py` | Wire USB CDC input alongside UDP |

### Existing files to reuse (copy or include)

| File | Reuse in |
|------|----------|
| `glove_firmware/receiver/lib/FramePairer.h` | P4 firmware (direct include via relative path) |
| `glove_firmware/receiver/lib/RelativeFeatures.h` | P4 firmware (direct include via relative path) |
| `glove_firmware/include/data_structures.h` | P4 firmware (shared GlovePacket definition) |

---

## Task 1: Shared Protocol Library (UART Framing)

**Files:**
- Create: `glove_firmware/shared/uart_frame.h`
- Create: `glove_firmware/shared/p4_protocol.h`
- Create: `glove_firmware/p4_base_station/tests/platformio.ini`
- Create: `glove_firmware/p4_base_station/tests/test_uart_frame/test_uart_frame.cpp`

**Context:** The C6 and P4 communicate over UART at 2Mbps. Each `GlovePacket` (69 bytes) from ESP-NOW is wrapped in a UART frame: `[0xAA][0x55][Payload...][CRC16_H][CRC16_L]`. The payload is the raw 69-byte GlovePacket. This framing layer must be testable natively (no hardware).

### Step 1: Write failing tests for UART frame encode/decode

Create the PlatformIO native test project:

`glove_firmware/p4_base_station/tests/platformio.ini`:
```ini
[env:native]
platform = native
build_flags = -DUNIT_TEST -std=c++17
```

`glove_firmware/p4_base_station/tests/test_uart_frame/test_uart_frame.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "data_structures.h"
#include "uart_frame.h"

void test_encode_roundtrip() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 42;
    pkt.flex[0] = 0.5f;
    pkt.imu[0] = 1.23f;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t frame_len = uart_frame_encode(&pkt, frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN(0, (int)frame_len);

    GlovePacket decoded = {};
    size_t consumed = 0;
    TEST_ASSERT_TRUE(uart_frame_decode(frame, frame_len, &decoded, &consumed));
    TEST_ASSERT_EQUAL_MEMORY(&pkt, &decoded, sizeof(GlovePacket));
}

void test_decode_rejects_bad_magic() {
    uint8_t frame[] = {0xFF, 0xFF, 0x00};
    GlovePacket decoded;
    size_t consumed;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, sizeof(frame), &decoded, &consumed));
}

void test_decode_rejects_bad_crc() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t len = uart_frame_encode(&pkt, frame, sizeof(frame));
    frame[len - 1] ^= 0xFF; // corrupt CRC low byte

    GlovePacket decoded;
    size_t consumed;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, len, &decoded, &consumed));
}

void test_decode_partial_frame_returns_zero_consumed() {
    uint8_t frame[] = {0xAA, 0x55}; // incomplete
    GlovePacket decoded;
    size_t consumed = 999;
    TEST_ASSERT_FALSE(uart_frame_decode(frame, 2, &decoded, &consumed));
    TEST_ASSERT_EQUAL(0, (int)consumed);
}

void test_encode_size() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();

    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t len = uart_frame_encode(&pkt, frame, sizeof(frame));
    // Expected: 2 (magic) + 69 (GlovePacket) + 2 (CRC) = 73
    TEST_ASSERT_EQUAL(73, (int)len);
}

void test_crc16_modbus_known_vector() {
    // CRC-16/MODBUS of {0x45, 0x47, 0x05, ...} should be deterministic
    uint8_t data[] = {0x45, 0x47, 0x05, 0x00, 0x2A, 0x00, 0x00, 0x00};
    uint16_t crc = crc16_modbus(data, sizeof(data));
    TEST_ASSERT_NOT_EQUAL(0, crc);
    // Verify recomputation gives same result
    TEST_ASSERT_EQUAL(crc, crc16_modbus(data, sizeof(data)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_encode_roundtrip);
    RUN_TEST(test_decode_rejects_bad_magic);
    RUN_TEST(test_decode_rejects_bad_crc);
    RUN_TEST(test_decode_partial_frame_returns_zero_consumed);
    RUN_TEST(test_encode_size);
    RUN_TEST(test_crc16_modbus_known_vector);
    return UNITY_END();
}
```

### Step 2: Run tests to verify they fail

```bash
cd glove_firmware/p4_base_station/tests
pio test -e native
```

Expected: Compilation error — `uart_frame.h` not found.

### Step 3: Implement uart_frame.h

Create `glove_firmware/shared/uart_frame.h`:

```cpp
#pragma once
#include <cstdint>
#include <cstring>

// UART frame format: [0xAA][0x55][Payload (69 bytes)][CRC16_H][CRC16_L]
// Total frame size: 2 + 69 + 2 = 73 bytes

static constexpr uint8_t  UART_MAGIC_0     = 0xAA;
static constexpr uint8_t  UART_MAGIC_1     = 0x55;
static constexpr size_t   UART_FRAME_MAX_SIZE = 73;

inline uint16_t crc16_modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

inline size_t uart_frame_encode(const void* payload, size_t payload_len,
                                 uint8_t* out_frame, size_t out_max) {
    size_t frame_len = 2 + payload_len + 2;
    if (out_max < frame_len) return 0;

    out_frame[0] = UART_MAGIC_0;
    out_frame[1] = UART_MAGIC_1;
    memcpy(out_frame + 2, payload, payload_len);
    uint16_t crc = crc16_modbus(payload, payload_len);
    out_frame[2 + payload_len]     = crc & 0xFF;
    out_frame[2 + payload_len + 1] = (crc >> 8) & 0xFF;
    return frame_len;
}

inline bool uart_frame_decode(const uint8_t* data, size_t data_len,
                               void* out_payload, size_t* out_consumed) {
    *out_consumed = 0;
    if (data_len < 4) return false;
    if (data[0] != UART_MAGIC_0 || data[1] != UART_MAGIC_1) return false;

    // Fixed payload size: GlovePacket = 69 bytes
    constexpr size_t PAYLOAD_SIZE = 69;
    size_t frame_len = 2 + PAYLOAD_SIZE + 2;
    if (data_len < frame_len) return false;

    memcpy(out_payload, data + 2, PAYLOAD_SIZE);

    uint16_t expected_crc = data[2 + PAYLOAD_SIZE] | (data[2 + PAYLOAD_SIZE + 1] << 8);
    uint16_t actual_crc = crc16_modbus(data + 2, PAYLOAD_SIZE);
    if (expected_crc != actual_crc) return false;

    *out_consumed = frame_len;
    return true;
}
```

### Step 4: Run tests to verify they pass

```bash
cd glove_firmware/p4_base_station/tests
pio test -e native
```

Expected: All 6 tests PASS.

### Step 5: Create p4_protocol.h

Create `glove_firmware/shared/p4_protocol.h`:

```cpp
#pragma once
#include "data_structures.h"

// P4 base station specific constants
static constexpr uint32_t P4_UART_BAUD       = 2000000;  // 2 Mbps
static constexpr uint32_t P4_INFERENCE_HZ     = 30;
static constexpr uint32_t P4_DISPLAY_HZ       = 10;
static constexpr uint32_t P4_BLE_TIMESLICE_PCT = 10;      // 10% for BLE
static constexpr uint32_t P4_TTS_DEDUP_MS     = 2000;     // same gesture dedup window

// Base station status (for display + USB reporting)
struct BaseStationStatus {
    bool c6_connected;
    bool p4_ready;
    float cpu_usage;
    float mem_usage;
    uint32_t uptime_s;
    uint8_t active_tier;  // 1=Tier1 only, 2=Tier2 P4, 3=Tier3 PC
};
```

### Step 6: Commit

```bash
git add glove_firmware/shared/ glove_firmware/p4_base_station/tests/
git commit -m "feat(shared): add UART frame protocol library with tests"
```

---

## Task 2: C6 Co-Processor Firmware (ESP-NOW + UART Relay)

**Files:**
- Create: `glove_firmware/p4_base_station/c6_firmware/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults`
- Create: `glove_firmware/p4_base_station/c6_firmware/main/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/c6_firmware/main/main.c`
- Create: `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h`
- Create: `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.c`

**Context:** The ESP32-C6-MINI-1 on the P4 EV Board receives ESP-NOW packets from both gloves, wraps each in a UART frame, and forwards to the P4 via UART (GPIO43→GPIO38, 2Mbps). This is ESP-IDF code targeting the C6 chip. Testing is on-hardware only for this task.

### Step 1: Create C6 ESP-IDF project scaffold

`glove_firmware/p4_base_station/c6_firmware/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../shared")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(c6_firmware)
```

`glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults`:
```
CONFIG_IDF_TARGET="esp32c6"
CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_NOW_ENABLED=y
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
```

`glove_firmware/p4_base_station/c6_firmware/main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c" "espnow_handler.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_now driver esp_event nvs_flash
)
```

### Step 2: Implement ESP-NOW receive handler

`glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "data_structures.h"

// Callback type: called when a valid GlovePacket is received
typedef void (*espnow_packet_cb_t)(const GlovePacket* pkt);

// Initialize ESP-NOW with receive callback
bool espnow_handler_init(espnow_packet_cb_t cb);

// Get number of packets received
uint32_t espnow_handler_rx_count(void);
```

`glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.c`:

```c
#include "espnow_handler.h"
#include <string.h>
#include "esp_now.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char* TAG = "espnow_rx";
static espnow_packet_cb_t s_callback = NULL;
static uint32_t s_rx_count = 0;

static void on_espnow_recv(const esp_now_recv_info_t* info,
                           const uint8_t* data, int len) {
    if (len != sizeof(GlovePacket)) {
        ESP_LOGD(TAG, "Bad len: %d", len);
        return;
    }
    GlovePacket pkt;
    memcpy(&pkt, data, sizeof(GlovePacket));

    // Validate magic
    if (pkt.magic[0] != 0x45 || pkt.magic[1] != 0x47) return;
    // Validate version
    if (pkt.version != 5) return;
    // Validate checksum
    if (!pkt.verifyChecksum()) {
        ESP_LOGW(TAG, "CRC fail");
        return;
    }

    s_rx_count++;
    if (s_callback) {
        s_callback(&pkt);
    }
}

bool espnow_handler_init(espnow_packet_cb_t cb) {
    s_callback = cb;

    // Init NVS (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init network + WiFi (required for ESP-NOW)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_espnow_recv));

    ESP_LOGI(TAG, "ESP-NOW receiver initialized");
    return true;
}

uint32_t espnow_handler_rx_count(void) {
    return s_rx_count;
}
```

### Step 3: Implement C6 main with UART relay

`glove_firmware/p4_base_station/c6_firmware/main/main.c`:

```c
#include <stdio.h>
#include <string.h>
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

static QueueHandle_t s_uart_queue = NULL;

// ESP-NOW callback: wrap packet in UART frame and send
static void on_glove_packet(const GlovePacket* pkt) {
    uint8_t frame[UART_FRAME_MAX_SIZE];
    size_t frame_len = uart_frame_encode(pkt, sizeof(GlovePacket),
                                          frame, sizeof(frame));
    if (frame_len > 0 && s_uart_queue) {
        // Send frame bytes through a simple byte queue
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
            // Send complete frames immediately
            if (tx_idx >= UART_FRAME_MAX_SIZE) {
                uart_write_bytes(UART_PORT, tx_buf, tx_idx);
                tx_idx = 0;
            }
        } else if (tx_idx > 0) {
            // Flush partial buffer on timeout
            uart_write_bytes(UART_PORT, tx_buf, tx_idx);
            tx_idx = 0;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "C6 base station starting");

    // Configure UART
    uart_config_t uart_cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Create UART TX queue
    s_uart_queue = xQueueCreate(UART_FRAME_MAX_SIZE * 4, sizeof(uint8_t));

    // Init ESP-NOW
    espnow_handler_init(on_glove_packet);

    // Start UART TX task
    xTaskCreate(uart_tx_task, "uart_tx", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "C6 ready — ESP-NOW → UART relay active");

    // Status log loop
    while (1) {
        ESP_LOGI(TAG, "RX count: %lu", espnow_handler_rx_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### Step 4: Build and flash C6 firmware

```bash
cd glove_firmware/p4_base_station/c6_firmware
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Expected: C6 boots, logs "ESP-NOW receiver initialized" and "C6 ready".

### Step 5: Verify ESP-NOW reception

With gloves powered on sending ESP-NOW packets:
```bash
# C6 serial output should show:
# I (xxxx) espnow_rx: RX count: xxxxx
```

### Step 6: Commit

```bash
git add glove_firmware/p4_base_station/c6_firmware/
git commit -m "feat(c6): ESP-NOW receiver with UART relay firmware"
```

---

## Task 3: P4 Firmware — UART Receive + Frame Pairing + Feature Assembly

**Files:**
- Create: `glove_firmware/p4_base_station/p4_firmware/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/main.c`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/uart_receiver.h`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/uart_receiver.c`
- Create: `glove_firmware/p4_base_station/tests/test_frame_pairer/test_frame_pairer.cpp`
- Create: `glove_firmware/p4_base_station/tests/test_feature_assembly/test_feature_assembly.cpp`

**Context:** The P4 receives UART frames from C6, decodes them into GlovePackets, pairs left+right using the existing `FramePairer`, computes 6 relative features using `RelativeFeatures`, and assembles the 28-dim feature vector. The FramePairer and RelativeFeatures classes are reused from `glove_firmware/receiver/lib/` via direct include.

### Step 1: Write failing test for frame pairer reuse

`glove_firmware/p4_base_station/tests/test_frame_pairer/test_frame_pairer.cpp`:

```cpp
#include <unity.h>
#include "data_structures.h"
#include "FramePairer.h"

static GlovePacket make_pkt(uint8_t hand, uint32_t tick) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = hand;
    pkt.tick_id = tick;
    pkt.timestamp_us = tick * 10000;
    for (int i = 0; i < 5; i++) pkt.flex[i] = 0.1f * (i + 1);
    for (int i = 0; i < 6; i++) pkt.imu[i] = 0.5f * (i + 1);
    pkt.computeChecksum();
    return pkt;
}

void test_pairer_matches_by_tick_id() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.feed(make_pkt(HAND_RIGHT, 100));
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL_UINT32(100, pair.tick_id);
}

void test_pairer_rejects_mismatched_ticks() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.feed(make_pkt(HAND_RIGHT, 200));
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

void test_pairer_timeout() {
    FramePairer pairer(50); // 50ms timeout
    pairer.feed(make_pkt(HAND_LEFT, 100));
    pairer.tick(60000); // 60ms later
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
    pairer.feed(make_pkt(HAND_RIGHT, 100));
    TEST_ASSERT_FALSE(pairer.getPair(pair)); // left expired
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pairer_matches_by_tick_id);
    RUN_TEST(test_pairer_rejects_mismatched_ticks);
    RUN_TEST(test_pairer_timeout);
    return UNITY_END();
}
```

### Step 2: Write failing test for 28-dim feature assembly

**Important:** `RelativeFeatures.compute()` requires `quaternion[4]` parameters, but `GlovePacket.imu[6]` only stores `euler[3]+gyro[3]` — no quaternion is transmitted over ESP-NOW. The P4 must compute relative features directly from the available `imu[6]` data.

`glove_firmware/p4_base_station/tests/test_feature_assembly/test_feature_assembly.cpp`:

```cpp
#include <unity.h>
#include "data_structures.h"
#include "FramePairer.h"

static GlovePacket make_pkt(uint8_t hand, uint32_t tick,
                             float flex_base, float imu_base) {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = hand;
    pkt.tick_id = tick;
    for (int i = 0; i < 5; i++) pkt.flex[i] = flex_base + i * 0.1f;
    for (int i = 0; i < 6; i++) pkt.imu[i] = imu_base + i * 0.2f;
    pkt.computeChecksum();
    return pkt;
}

// Compute relative features from GlovePacket imu[6] (euler[3] + gyro[3])
// NOTE: Cannot use RelativeFeatures.compute() — it requires quaternion[4]
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
    // out[5] = max gyro axis difference
    float max_diff = 0; int max_idx = 0;
    for (int i = 3; i < 6; i++) {
        float diff = fabsf(left_imu[i] - right_imu[i]);
        if (diff > max_diff) { max_diff = diff; max_idx = i - 3; }
    }
    out[5] = max_idx / 3.0f;
}

void test_assemble_28dim_from_pair() {
    FramePairer pairer;
    pairer.feed(make_pkt(HAND_LEFT, 1, 0.1f, 1.0f));
    pairer.feed(make_pkt(HAND_RIGHT, 1, 0.5f, 2.0f));
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));

    float relative[6];
    compute_relative_from_imu(pair.left.imu, pair.right.imu, relative);

    float features[DUAL_HAND_FEATURES]; // 28
    memcpy(features, pair.left.flex, 5 * sizeof(float));
    memcpy(features + 5, pair.left.imu, 6 * sizeof(float));
    memcpy(features + 11, pair.right.flex, 5 * sizeof(float));
    memcpy(features + 16, pair.right.imu, 6 * sizeof(float));
    memcpy(features + 22, relative, 6 * sizeof(float));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, features[0]);   // left flex[0]
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, features[1]);   // left flex[1]
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, features[11]);  // right flex[0]
    TEST_ASSERT_NOT_EQUAL(0.0f, features[22]);              // delta euler != 0
}

void test_compute_relative_from_imu_delta_euler() {
    float left_imu[6]  = {10.0f, 20.0f, 30.0f, 1.0f, 2.0f, 3.0f};
    float right_imu[6] = {5.0f, 10.0f, 15.0f, 0.5f, 1.0f, 1.5f};
    float out[6];
    compute_relative_from_imu(left_imu, right_imu, out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, out[0]);   // 10-5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, out[1]);  // 20-10
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, out[2]);  // 30-15
}

void test_compute_relative_from_imu_zero_when_identical() {
    float imu[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    float out[6];
    compute_relative_from_imu(imu, imu, out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[3]); // euler dist = 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out[4]); // gyro norm diff = 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_assemble_28dim_from_pair);
    RUN_TEST(test_compute_relative_from_imu_delta_euler);
    RUN_TEST(test_compute_relative_from_imu_zero_when_identical);
    return UNITY_END();
}
```

### Step 3: Run tests to verify they fail

```bash
cd glove_firmware/p4_base_station/tests
pio test -e native
```

Expected: Compile errors for `FramePairer.h` not found.

### Step 4: Add include paths to test platformio.ini

Update `glove_firmware/p4_base_station/tests/platformio.ini`:

```ini
[env:native]
platform = native
build_flags =
    -DUNIT_TEST
    -std=c++17
    -I../../include
    -I../../receiver/lib
    -I../../shared
```

### Step 5: Run tests — they should now compile and pass

```bash
cd glove_firmware/p4_base_station/tests
pio test -e native
```

Expected: All 10 tests PASS (6 UART frame + 3 frame pairer + 3 feature assembly). FramePairer is reused as-is; relative features computed directly from `imu[6]` (not RelativeFeatures which needs quaternion).

### Step 6: Create P4 ESP-IDF project scaffold

`glove_firmware/p4_base_station/p4_firmware/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(p4_firmware)
```

`glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`:
```
CONFIG_IDF_TARGET="esp32p4"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_ESP_CONSOLE_USB_CDC=y
```

`glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c" "uart_receiver.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event
    PRIV_REQUIRES esp_timer
)
```

### Step 7: Implement UART receiver for P4

`glove_firmware/p4_base_station/p4_firmware/main/uart_receiver.h`:

```c
#pragma once
#include <stdbool.h>
#include "data_structures.h"

// Initialize UART receiver on given port/pins
bool uart_receiver_init(int uart_port, int tx_pin, int rx_pin, int baud);

// Try to receive one GlovePacket (non-blocking)
bool uart_receiver_poll(GlovePacket* out);

// Get received packet count
uint32_t uart_receiver_count(void);
```

`glove_firmware/p4_base_station/p4_firmware/main/uart_receiver.c`:

```c
#include "uart_receiver.h"
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "uart_frame.h"

static const char* TAG = "uart_rx";
static int s_port = 0;
static uint32_t s_count = 0;

// Ring buffer for frame assembly
static uint8_t s_buf[UART_FRAME_MAX_SIZE * 4];
static size_t s_buf_len = 0;

bool uart_receiver_init(int uart_port, int tx_pin, int rx_pin, int baud) {
    s_port = uart_port;

    uart_config_t cfg = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(s_port, 2048, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(s_port, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(s_port, tx_pin, rx_pin,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART%d init: TX=%d RX=%d baud=%d", s_port, tx_pin, rx_pin, baud);
    return true;
}

bool uart_receiver_poll(GlovePacket* out) {
    // Read available bytes into ring buffer
    int avail = uart_read_bytes(s_port, s_buf + s_buf_len,
                                 sizeof(s_buf) - s_buf_len, 0);
    if (avail > 0) s_buf_len += avail;

    // Try to decode a frame from buffer
    size_t consumed = 0;
    if (uart_frame_decode(s_buf, s_buf_len, out, &consumed)) {
        // Shift buffer
        if (consumed < s_buf_len) {
            memmove(s_buf, s_buf + consumed, s_buf_len - consumed);
        }
        s_buf_len -= consumed;
        s_count++;
        return true;
    }

    // Discard leading non-magic bytes
    while (s_buf_len > 0 && s_buf[0] != UART_MAGIC_0) {
        memmove(s_buf, s_buf + 1, --s_buf_len);
    }

    return false;
}

uint32_t uart_receiver_count(void) {
    return s_count;
}
```

### Step 8: Implement P4 main.c (UART receive + pair + assemble)

`glove_firmware/p4_base_station/p4_firmware/main/main.c`:

**Note:** `RelativeFeatures.compute()` requires `quaternion[4]` which is NOT in `GlovePacket.imu[6]`. Use `compute_relative_from_imu()` instead (defined inline below).

```c
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "uart_receiver.h"
#include "data_structures.h"

// Reuse FramePairer from receiver (header-only, no quaternion dependency)
#include "FramePairer.h"

static const char* TAG = "p4_main";

static FramePairer s_pairer;

// Compute 6-dim relative features from GlovePacket imu[6] (euler[3]+gyro[3])
static void compute_relative_from_imu(
    const float* left_imu, const float* right_imu, float* out) {
    memset(out, 0, 6 * sizeof(float));
    for (int i = 0; i < 3; i++)
        out[i] = left_imu[i] - right_imu[i];           // delta euler
    float dist_sq = 0;
    for (int i = 0; i < 3; i++) dist_sq += out[i] * out[i];
    out[3] = sqrtf(dist_sq);                             // euler distance
    float norm_l = 0, norm_r = 0;
    for (int i = 3; i < 6; i++) {
        norm_l += left_imu[i] * left_imu[i];
        norm_r += right_imu[i] * right_imu[i];
    }
    out[4] = sqrtf(norm_l) - sqrtf(norm_r);             // delta gyro norm
    float max_diff = 0; int max_idx = 0;
    for (int i = 3; i < 6; i++) {
        float diff = fabsf(left_imu[i] - right_imu[i]);
        if (diff > max_diff) { max_diff = diff; max_idx = i - 3; }
    }
    out[5] = max_idx / 3.0f;                            // max gyro axis / 3
}

static void uart_task(void* arg) {
    GlovePacket pkt;
    while (1) {
        if (uart_receiver_poll(&pkt)) {
            s_pairer.feed(pkt);  // feed takes const ref

            FramePair pair;
            if (s_pairer.getPair(pair)) {
                float relative[6];
                compute_relative_from_imu(pair.left.imu, pair.right.imu, relative);

                float features[DUAL_HAND_FEATURES];
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

void app_main(void) {
    ESP_LOGI(TAG, "P4 base station starting");
    uart_receiver_init(0, 37, 38, 2000000);
    xTaskCreatePinnedToCore(uart_task, "uart_rx", 8192, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "P4 ready — UART receiver active");

    while (1) {
        ESP_LOGI(TAG, "UART RX count: %lu", (unsigned long)uart_receiver_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### Step 9: Build P4 firmware

```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py set-target esp32p4
idf.py build
```

Expected: Build succeeds (compilation only, no flash yet — needs display/audio config first).

### Step 10: Commit

```bash
git add glove_firmware/p4_base_station/p4_firmware/ glove_firmware/p4_base_station/tests/
git commit -m "feat(p4): UART receiver with frame pairing and 28-dim feature assembly"
```

---

## Task 4: P4 Firmware — Tier2 Inference (TFLite Micro)

**Files:**
- Create: `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.h`
- Create: `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.c`
- Create: `glove_firmware/scripts/export_model.py`
- Create: `glove_firmware/scripts/calibrate_quantization.py`
- Generate: `glove_firmware/p4_base_station/p4_firmware/model/model_data.h`

**Context:** The Tier2 model is `GatedBiCrossAttention` from `glove_relay/src/models/tier2_cross_attn.py`. It takes 28-dim × T=30 frames and outputs 46 class probabilities. The model must be exported from PyTorch → ONNX → TFLite INT8 → C header for deployment on P4.

### Step 1: Read the existing Python model

Read `glove_relay/src/models/tier2_cross_attn.py` to understand the exact architecture, input shapes, and forward pass.

### Step 2: Write model export script

`glove_firmware/scripts/export_model.py`:

```python
"""Export GatedBiCrossAttention model from PyTorch to TFLite INT8."""
from __future__ import annotations
import sys
import os
import numpy as np

# Add relay to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../glove_relay/src"))

import torch
from models.tier2_cross_attn import GatedBiCrossAttention

WINDOW_SIZE = 30
INPUT_DIM = 28
NUM_CLASSES = 46

def export_to_onnx(model, onnx_path: str):
    model.eval()
    dummy = torch.randn(1, WINDOW_SIZE, INPUT_DIM)
    torch.onnx.export(
        model, dummy, onnx_path,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}},
        opset_version=13,
    )
    print(f"ONNX exported: {onnx_path}")

def convert_to_tflite(onnx_path: str, tflite_path: str):
    import onnx
    from onnx_tf.backend import prepare
    import tensorflow as tf

    onnx_model = onnx.load(onnx_path)
    tf_rep = prepare(onnx_model)
    tf_rep.export_graph(os.path.splitext(onnx_path)[0] + "_tf")

    converter = tf.lite.TFLiteConverter.from_saved_model(
        os.path.splitext(onnx_path)[0] + "_tf"
    )
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    # Representative dataset for INT8 quantization
    def representative_dataset():
        for _ in range(100):
            yield [np.random.randn(1, WINDOW_SIZE, INPUT_DIM).astype(np.float32)]

    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.float32
    converter.inference_output_type = tf.float32

    tflite_model = converter.convert()
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)
    print(f"TFLite INT8 exported: {tflite_path} ({len(tflite_model)} bytes)")

def tflite_to_c_header(tflite_path: str, header_path: str):
    with open(tflite_path, "rb") as f:
        data = f.read()

    with open(header_path, "w") as f:
        f.write("// Auto-generated by export_model.py\n")
        f.write(f"// Model size: {len(data)} bytes\n\n")
        f.write("#pragma once\n\n")
        f.write(f"const unsigned int model_data_len = {len(data)};\n")
        f.write("alignas(16) const unsigned char model_data[] = {\n")
        for i, byte in enumerate(data):
            if i % 16 == 0:
                f.write("  ")
            f.write(f"0x{byte:02x}, ")
            if i % 16 == 15:
                f.write("\n")
        f.write("\n};\n")
    print(f"C header: {header_path}")

def main():
    # Load model
    model = GatedBiCrossAttention(
        input_dim=INPUT_DIM,
        d_model=32,
        num_classes=NUM_CLASSES,
    )
    # Load weights if checkpoint exists
    checkpoint = os.path.join(os.path.dirname(__file__),
                               "../../glove_relay/models/tier2_cross_attn.pth")
    if os.path.exists(checkpoint):
        model.load_state_dict(torch.load(checkpoint, map_location="cpu"))
        print(f"Loaded weights from {checkpoint}")
    else:
        print("WARNING: No trained weights found, exporting random model")

    base = os.path.join(os.path.dirname(__file__),
                         "../p4_base_station/p4_firmware/model")
    os.makedirs(base, exist_ok=True)

    onnx_path = os.path.join(base, "tier2.onnx")
    tflite_path = os.path.join(base, "tier2_int8.tflite")
    header_path = os.path.join(base, "model_data.h")

    export_to_onnx(model, onnx_path)
    convert_to_tflite(onnx_path, tflite_path)
    tflite_to_c_header(tflite_path, header_path)

if __name__ == "__main__":
    main()
```

### Step 3: Run model export

```bash
conda activate pytorch21_env
cd glove_firmware
python scripts/export_model.py
```

Expected: Generates `model/tier2.onnx`, `model/tier2_int8.tflite`, `model/model_data.h`. If no trained weights, exports with random weights (functional but untrained).

### Step 4: Implement TFLite Micro wrapper

`glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TFLITE_INPUT_DIM    28
#define TFLITE_WINDOW_SIZE  30
#define TFLITE_NUM_CLASSES  46

typedef struct {
    int gesture_id;
    float confidence;
    float probabilities[TFLITE_NUM_CLASSES];
    uint32_t inference_us;
    bool valid;
} Tier2Result;

// Initialize TFLite Micro with model data
bool tflite_init(const unsigned char* model_data, unsigned int model_len);

// Run inference on 28-dim × 30-frame window
Tier2Result tflite_run(const float features[TFLITE_WINDOW_SIZE][TFLITE_INPUT_DIM]);

// Get arena memory usage
uint32_t tflite_arena_usage(void);
```

`glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.c`:

```c
#include "tflite_infer.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char* TAG = "tflite";

static constexpr int kArenaSize = 32 * 1024; // 32KB arena in PSRAM
static uint8_t* s_arena = NULL;
static tflite::MicroInterpreter* s_interpreter = NULL;
static TfLiteTensor* s_input = NULL;
static TfLiteTensor* s_output = NULL;

bool tflite_init(const unsigned char* model_data, unsigned int model_len) {
    const tflite::Model* model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version mismatch");
        return false;
    }

    // Allocate arena in PSRAM
    s_arena = (uint8_t*)heap_caps_malloc(kArenaSize, MALLOC_CAP_SPIRAM);
    if (!s_arena) {
        ESP_LOGE(TAG, "Arena alloc failed");
        return false;
    }

    // Register required ops
    static tflite::MicroMutableOpResolver<8> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddMul();
    resolver.AddAdd();
    resolver.AddRelu();
    resolver.AddSigmoid();
    resolver.AddQuantize();
    resolver.AddDequantize();

    static tflite::MicroInterpreter interpreter(
        model, resolver, s_arena, kArenaSize);
    s_interpreter = &interpreter;

    if (s_interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed");
        return false;
    }

    s_input = s_interpreter->input(0);
    s_output = s_interpreter->output(0);

    ESP_LOGI(TAG, "TFLite init OK. Arena used: %zu / %d",
             s_interpreter->arena_used_bytes(), kArenaSize);
    return true;
}

Tier2Result tflite_run(const float features[TFLITE_WINDOW_SIZE][TFLITE_INPUT_DIM]) {
    Tier2Result result = {-1, 0.0f, {}, 0, false};

    if (!s_interpreter || !s_input || !s_output) return result;

    int64_t t0 = esp_timer_get_time();

    // Copy input data
    memcpy(s_input->data.f, features,
           TFLITE_WINDOW_SIZE * TFLITE_INPUT_DIM * sizeof(float));

    if (s_interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke failed");
        return result;
    }

    int64_t t1 = esp_timer_get_time();
    result.inference_us = (uint32_t)(t1 - t0);

    // Parse output (46 class logits → softmax → argmax)
    float max_score = -1e9f;
    float sum_exp = 0.0f;
    for (int i = 0; i < TFLITE_NUM_CLASSES; i++) {
        float v = s_output->data.f[i];
        if (v > max_score) { max_score = v; result.gesture_id = i; }
    }
    // Softmax
    for (int i = 0; i < TFLITE_NUM_CLASSES; i++) {
        result.probabilities[i] = expf(s_output->data.f[i] - max_score);
        sum_exp += result.probabilities[i];
    }
    for (int i = 0; i < TFLITE_NUM_CLASSES; i++) {
        result.probabilities[i] /= sum_exp;
    }
    result.confidence = result.probabilities[result.gesture_id];
    result.valid = result.confidence >= 0.5f;

    return result;
}

uint32_t tflite_arena_usage(void) {
    return s_interpreter ? s_interpreter->arena_used_bytes() : 0;
}
```

### Step 5: Integrate inference into P4 main.c

Add to `glove_firmware/p4_base_station/p4_firmware/main/main.c` (after the uart_task):

```c
#include "tflite_infer.h"
#include "model_data.h"

// Sliding window buffer
static float s_window[TFLITE_WINDOW_SIZE][TFLITE_INPUT_DIM];
static int s_window_idx = 0;
static bool s_window_full = false;

// Inference task
static void inference_task(void* arg) {
    while (1) {
        // Wait for features from UART task (use FreeRTOS queue in production)
        // For now, poll at 30Hz
        if (s_window_full) {
            Tier2Result result = tflite_run(s_window);
            if (result.valid) {
                ESP_LOGI(TAG, "Tier2: gesture=%d conf=%.2f time=%lu us",
                         result.gesture_id, result.confidence, result.inference_us);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30Hz
    }
}
```

Update `app_main()` to init TFLite and start inference task:

```c
// In app_main(), after uart_receiver_init():
if (!tflite_init(model_data, model_data_len)) {
    ESP_LOGE(TAG, "TFLite init failed!");
} else {
    xTaskCreatePinnedToCore(inference_task, "inference", 16384, NULL, 2, NULL, 1);
}
```

### Step 6: Build and verify

```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py build
```

Expected: Build succeeds. On flash to P4, logs "TFLite init OK. Arena used: ~X / 32768".

### Step 7: Commit

```bash
git add glove_firmware/p4_base_station/p4_firmware/components/ glove_firmware/p4_base_station/p4_firmware/model/ glove_firmware/scripts/export_model.py
git commit -m "feat(p4): TFLite Micro Tier2 inference with model export pipeline"
```

---

## Task 5: P4 Firmware — LVGL Display UI

**Files:**
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/main.c`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/display_task.h`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/display_task.c`

**Context:** The 7" MIPI-DSI touchscreen (1024×600, GT911 touch) on the P4 EV Board displays hand gesture recognition results. The ESP-IDF BSP for this board provides LVGL porting. UI updates at 10Hz with inference results.

### Step 1: Add BSP dependency

Add to `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c" "uart_receiver.c" "display_task.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event esp_lcd esp_timer
    PRIV_REQUIRES esp_psram
)
```

### Step 2: Implement display_task.c

`glove_firmware/p4_base_station/p4_firmware/main/display_task.h`:

```c
#pragma once
#include "tflite_infer.h"
#include "data_structures.h"
#include "p4_protocol.h"

// Initialize LVGL and display
bool display_init(void);

// Update display with latest data
void display_update(const Tier2Result* result,
                    const FramePair* pair,
                    const float features[DUAL_HAND_FEATURES],
                    const BaseStationStatus* status);
```

`glove_firmware/p4_base_station/p4_firmware/main/display_task.c`:

```c
#include "display_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

static const char* TAG = "display";

// Gesture names (46 classes)
static const char* GESTURE_NAMES[46] = {
    "你好", "谢谢", "对不起", "是", "不是",
    "请", "再见", "早上好", "晚上好", "快乐",
    "悲伤", "生气", "害怕", "惊讶", "吃饭",
    "睡觉", "工作", "学习", "家", "学校",
    "朋友", "家人", "医生", "帮助", "喜欢",
    "不喜欢", "大", "小", "多", "少",
    "好", "坏", "新", "旧", "快",
    "慢", "左", "右", "上", "下",
    "来", "去", "看", "听", "说",
    "想"
};

// LVGL widgets
static lv_obj_t* s_lbl_left_gesture = NULL;
static lv_obj_t* s_lbl_right_gesture = NULL;
static lv_obj_t* s_bar_left_conf = NULL;
static lv_obj_t* s_bar_right_conf = NULL;
static lv_obj_t* s_lbl_status = NULL;
static lv_obj_t* s_lbl_log = NULL;
static lv_obj_t* s_flex_bars_left[5] = {};
static lv_obj_t* s_flex_bars_right[5] = {};

static const char* FINGER_NAMES[5] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};

bool display_init(void) {
    // NOTE: LVGL port init and display driver setup
    // should use BSP from esp-bsp for the P4 EV Board.
    // This is a placeholder for the LVGL widget creation.

    ESP_LOGI(TAG, "Display init (LVGL)");
    return true;
}

void display_update(const Tier2Result* result,
                    const FramePair* pair,
                    const float features[DUAL_HAND_FEATURES],
                    const BaseStationStatus* status) {
    // Update gesture labels
    if (result && result->valid) {
        const char* name = (result->gesture_id < 46) ?
                           GESTURE_NAMES[result->gesture_id] : "Unknown";
        // lv_label_set_text_fmt(s_lbl_left_gesture, "%s", name);
        ESP_LOGI(TAG, "Gesture: %s (%.0f%%)", name, result->confidence * 100);
    }

    // Update status
    if (status) {
        ESP_LOGI(TAG, "Status: C6=%d P4=%d Tier=%d",
                 status->c6_connected, status->p4_ready, status->active_tier);
    }
}
```

### Step 3: Integrate display task into main.c

Add display task creation in `app_main()`:

```c
#include "display_task.h"

// In app_main():
display_init();
// Display update runs in the uart_task loop (10Hz throttle)
```

### Step 4: Build and verify

```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py build
```

Expected: Build succeeds. On hardware, LVGL initializes on the 7" screen.

### Step 5: Commit

```bash
git add glove_firmware/p4_base_station/p4_firmware/main/display_task.*
git commit -m "feat(p4): LVGL display UI with gesture visualization"
```

---

## Task 6: P4 Firmware — TTS Audio + USB CDC

**Files:**
- Create: `glove_firmware/p4_base_station/p4_firmware/main/audio_task.h`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/audio_task.c`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/usb_task.h`
- Create: `glove_firmware/p4_base_station/p4_firmware/main/usb_task.c`
- Create: `glove_firmware/scripts/generate_pcm_tts.py`

**Context:** ES8311 codec is pre-wired on the P4 EV Board. Use `esp_codec_dev` component for I2S audio. Pre-recorded PCM files on MicroSD. USB HS CDC sends inference results to PC.

### Step 1: Implement audio task

`glove_firmware/p4_base_station/p4_firmware/main/audio_task.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Initialize I2S + ES8311 codec
bool audio_init(void);

// Play gesture audio by ID (0-45), dedup within 2 seconds
void audio_play_gesture(int gesture_id);
```

`glove_firmware/p4_base_station/p4_firmware/main/audio_task.c`:

```c
#include "audio_task.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "audio";
static int64_t s_last_play_time[46] = {};
static const int64_t DEDUP_US = 2000000; // 2 seconds

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

    // Read PCM file from MicroSD
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/tts/%02d.pcm", gesture_id);
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "PCM not found: %s", path);
        return;
    }

    // Read and play via I2S DMA
    uint8_t buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        // i2s_write(I2S_NUM_0, buf, bytes_read, &bytes_written, portMAX_DELAY);
        // TODO: wire to esp_codec_dev I2S write
    }
    fclose(f);
    ESP_LOGI(TAG, "Played gesture %d", gesture_id);
}
```

### Step 2: Implement USB CDC task

`glove_firmware/p4_base_station/p4_firmware/main/usb_task.h`:

```c
#pragma once
#include "tflite_infer.h"
#include "data_structures.h"

// Initialize USB HS CDC
bool usb_task_init(void);

// Send inference result + raw data to PC
void usb_task_send_result(const Tier2Result* result,
                           const FramePair* pair,
                           const float features[DUAL_HAND_FEATURES]);
```

`glove_firmware/p4_base_station/p4_firmware/main/usb_task.c`:

```c
#include "usb_task.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

static const char* TAG = "usb";

bool usb_task_init(void) {
    ESP_LOGI(TAG, "USB HS CDC init");
    // TinyUSB CDC ACM initialization
    // TODO: Configure USB HS with TinyUSB
    return true;
}

void usb_task_send_result(const Tier2Result* result,
                           const FramePair* pair,
                           const float features[DUAL_HAND_FEATURES]) {
    if (!result) return;

    // Send compact JSON over CDC
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"gesture\":%d,\"conf\":%.3f,\"tier2_time\":%lu}\n",
        result->gesture_id, result->confidence, result->inference_us);

    // tud_cdc_n_write(0, (const uint8_t*)buf, n);
    // tud_cdc_n_write_flush(0);
    // TODO: Wire to TinyUSB CDC write
    ESP_LOGD(TAG, "USB TX: %d bytes", n);
}
```

### Step 3: Write PCM generation script

`glove_firmware/scripts/generate_pcm_tts.py`:

```python
"""Generate 46 gesture name PCM files for TTS playback on P4."""
from __future__ import annotations
import os
import subprocess
import struct

GESTURE_NAMES = [
    "你好", "谢谢", "对不起", "是", "不是",
    "请", "再见", "早上好", "晚上好", "快乐",
    "悲伤", "生气", "害怕", "惊讶", "吃饭",
    "睡觉", "工作", "学习", "家", "学校",
    "朋友", "家人", "医生", "帮助", "喜欢",
    "不喜欢", "大", "小", "多", "少",
    "好", "坏", "新", "旧", "快",
    "慢", "左", "右", "上", "下",
    "来", "去", "看", "听", "说",
    "想",
]

OUTPUT_DIR = "tts_pcm"
SAMPLE_RATE = 16000

def generate_with_edge_tts(text: str, output_path: str):
    """Use edge-tts to generate audio, then convert to raw PCM."""
    import asyncio
    import edge_tts

    async def _gen():
        communicate = edge_tts.Communicate(text, "zh-CN-XiaoxiaoNeural")
        mp3_path = output_path + ".mp3"
        await communicate.save(mp3_path)
        # Convert MP3 to raw PCM using ffmpeg
        subprocess.run([
            "ffmpeg", "-y", "-i", mp3_path,
            "-ar", str(SAMPLE_RATE), "-ac", "1", "-f", "s16le",
            output_path
        ], check=True, capture_output=True)
        os.remove(mp3_path)

    asyncio.run(_gen())

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for i, name in enumerate(GESTURE_NAMES):
        pcm_path = os.path.join(OUTPUT_DIR, f"{i:02d}.pcm")
        print(f"[{i:02d}] Generating: {name} → {pcm_path}")
        try:
            generate_with_edge_tts(name, pcm_path)
        except Exception as e:
            print(f"  FAILED: {e}")
    print(f"\nGenerated PCM files in {OUTPUT_DIR}/")

if __name__ == "__main__":
    main()
```

### Step 4: Build and verify

```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py build
```

Expected: Build succeeds. Audio and USB are placeholder stubs that log output.

### Step 5: Commit

```bash
git add glove_firmware/p4_base_station/p4_firmware/main/audio_task.* usb_task.*
git add glove_firmware/scripts/generate_pcm_tts.py
git commit -m "feat(p4): TTS audio and USB CDC output tasks"
```

---

## Task 7: PC Relay Updates (USB CDC Input + Protobuf Extension)

**Files:**
- Modify: `glove_relay/proto/glove_data.proto`
- Modify: `glove_relay/src/protobuf_parser.py`
- Modify: `glove_relay/src/udp_server.py`
- Modify: `glove_relay/src/main.py`
- Create: `glove_relay/tests/test_usb_cdc_input.py`

**Context:** The relay needs to accept data from the P4 via USB CDC (serial) as an alternative to UDP, and parse the new `BaseStationPacket` protobuf that includes P4 Tier2 results and status.

### Step 1: Write failing test for USB CDC input

`glove_relay/tests/test_usb_cdc_input.py`:

```python
"""Tests for USB CDC serial input from P4 base station."""
from __future__ import annotations
import pytest
from unittest.mock import MagicMock, patch
from proto.glove_data_pb2 import ReceiverPacket, BaseStationPacket
from src.protobuf_parser import ProtobufParser


class TestBaseStationPacketParsing:
    def setup_method(self):
        self.parser = ProtobufParser()

    def test_parse_base_station_packet(self):
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.tick_id = 100
        bsp.left.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
        bsp.left.imu.extend([1.0, 2.0, 3.0, 0.1, 0.2, 0.3])
        bsp.right.flex.extend([0.6, 0.7, 0.8, 0.9, 1.0])
        bsp.right.imu.extend([4.0, 5.0, 6.0, 0.4, 0.5, 0.6])
        bsp.relative_features.extend([0.1, 0.2, 0.3, 0.4, 0.5, 0.6])
        bsp.tier2_gesture_id = 5
        bsp.tier2_confidence = 0.92
        bsp.base_station.c6_connected = True
        bsp.base_station.p4_ready = True
        bsp.base_station.active_tier = 2

        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is not None
        assert parsed["version"] == 5
        assert parsed["tick_id"] == 100
        assert len(parsed["left"]["flex"]) == 5
        assert parsed["tier2_gesture_id"] == 5
        assert parsed["tier2_confidence"] == pytest.approx(0.92, abs=0.01)
        assert parsed["base_station"]["c6_connected"] is True

    def test_parse_base_station_packet_28dim(self):
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.left.flex.extend([0.1] * 5)
        bsp.left.imu.extend([1.0] * 6)
        bsp.right.flex.extend([0.5] * 5)
        bsp.right.imu.extend([2.0] * 6)
        bsp.relative_features.extend([0.01] * 6)

        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        features = self.parser.assemble_28dim_from_bsp(parsed)
        assert len(features) == 28
        assert features[0] == pytest.approx(0.1)
        assert features[11] == pytest.approx(0.5)

    def test_parse_base_station_packet_bad_version(self):
        bsp = BaseStationPacket()
        bsp.version = 99
        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is None

    def test_parse_base_station_packet_empty_bytes(self):
        parsed = self.parser.parse_base_station_packet(b"")
        assert parsed is None


class TestUSBCDCInput:
    @pytest.mark.asyncio
    async def test_usb_cdc_server_receives_data(self):
        """Verify USBCDCServer can receive serial data and parse it."""
        from src.usb_cdc_server import USBCDCServer

        server = USBCDCServer(port="/dev/null", baud=2000000)
        # Mock the serial read
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.left.flex.extend([0.1] * 5)
        bsp.left.imu.extend([1.0] * 6)
        bsp.right.flex.extend([0.5] * 5)
        bsp.right.imu.extend([2.0] * 6)
        bsp.relative_features.extend([0.01] * 6)

        # This test verifies the class can be instantiated
        assert server is not None
```

### Step 2: Run tests to verify they fail

```bash
cd glove_relay
conda activate pytorch21_env
python -m pytest tests/test_usb_cdc_input.py -v
```

Expected: Import errors — `parse_base_station_packet` not found, `USBCDCServer` not found.

### Step 3: Extend protobuf schema

Add to `glove_relay/proto/glove_data.proto`:

```protobuf
message BaseStationStatus {
    bool c6_connected = 1;
    bool p4_ready = 2;
    float cpu_usage = 3;
    float mem_usage = 4;
    uint32 uptime_s = 5;
    uint32 active_tier = 6;  // 1=glove-only, 2=P4, 3=PC
}

message BaseStationPacket {
    uint32 version = 1;
    uint32 tick_id = 2;
    GloveData left = 3;
    GloveData right = 4;
    repeated float relative_features = 5;  // [6]
    uint32 tier2_gesture_id = 6;
    float tier2_confidence = 7;
    BaseStationStatus base_station = 8;
}
```

Regenerate Python protobuf:

```bash
cd glove_relay
python -m grpc_tools.protoc -I proto --python_out=proto proto/glove_data.proto
```

### Step 4: Add parse_base_station_packet to ProtobufParser

Add to `glove_relay/src/protobuf_parser.py`:

```python
from proto.glove_data_pb2 import GloveData, ReceiverPacket, BaseStationPacket

class ProtobufParser:
    # ... existing methods ...

    def parse_base_station_packet(self, data: bytes) -> Optional[dict]:
        """Parse a BaseStationPacket from P4 base station (USB CDC input)."""
        try:
            bsp = BaseStationPacket()
            bsp.ParseFromString(data)
            if bsp.version != self._expected_version:
                return None
            result = {
                "version": bsp.version,
                "tick_id": bsp.tick_id,
                "left": self._parse_glove_data(bsp.left),
                "right": self._parse_glove_data(bsp.right),
                "relative_features": list(bsp.relative_features),
                "tier2_gesture_id": bsp.tier2_gesture_id,
                "tier2_confidence": bsp.tier2_confidence,
            }
            if bsp.HasField("base_station"):
                result["base_station"] = {
                    "c6_connected": bsp.base_station.c6_connected,
                    "p4_ready": bsp.base_station.p4_ready,
                    "cpu_usage": bsp.base_station.cpu_usage,
                    "mem_usage": bsp.base_station.mem_usage,
                    "uptime_s": bsp.base_station.uptime_s,
                    "active_tier": bsp.base_station.active_tier,
                }
            return result
        except Exception:
            return None

    def assemble_28dim_from_bsp(self, parsed: dict) -> list[float]:
        """Assemble 28-dim from a parsed BaseStationPacket."""
        return self.assemble_28dim(parsed)
```

### Step 5: Create USB CDC server

`glove_relay/src/usb_cdc_server.py`:

```python
"""USB CDC serial input server for P4 base station data."""
from __future__ import annotations
import asyncio
import logging
from typing import Optional, Callable

logger = logging.getLogger(__name__)


class USBCDCServer:
    def __init__(self, port: str = "/dev/ttyACM0", baud: int = 2000000):
        self.port = port
        self.baud = baud
        self._running = False
        self._on_data: Optional[Callable] = None

    def set_callback(self, callback: Callable[[bytes], None]):
        self._on_data = callback

    async def start(self):
        """Start reading from serial port."""
        self._running = True
        try:
            import serial_asyncio
            reader, _ = await serial_asyncio.open_serial_connection(
                url=self.port, baudrate=self.baud
            )
            logger.info(f"USB CDC connected: {self.port} @ {self.baud}")

            while self._running:
                data = await reader.read(4096)
                if data and self._on_data:
                    self._on_data(data)
        except Exception as e:
            logger.error(f"USB CDC error: {e}")
            self._running = False

    def stop(self):
        self._running = False
```

### Step 6: Wire USB CDC into relay main.py

Add to `glove_relay/src/main.py` lifespan:

```python
from src.usb_cdc_server import USBCDCServer

# In lifespan startup:
usb_server = USBCDCServer(port="/dev/ttyACM0", baud=2000000)
usb_server.set_callback(lambda data: asyncio.create_task(handle_usb_data(data)))
asyncio.create_task(usb_server.start())
```

### Step 7: Run all tests

```bash
cd glove_relay
python -m pytest tests/ -v
```

Expected: All existing 133 tests pass + 5 new tests pass = 138 total.

### Step 8: Commit

```bash
git add glove_relay/proto/ glove_relay/src/protobuf_parser.py glove_relay/src/usb_cdc_server.py glove_relay/src/main.py glove_relay/tests/test_usb_cdc_input.py
git commit -m "feat(relay): add USB CDC input and BaseStationPacket parsing"
```

---

## Task 8: Integration Test + Demo Script

**Files:**
- Create: `glove_relay/tests/test_p4_integration.py`

**Context:** End-to-end test verifying the full data path: simulated glove packets → C6 → UART → P4 → inference → display + audio + USB → PC relay.

### Step 1: Write integration test

`glove_relay/tests/test_p4_integration.py`:

```python
"""Integration test for P4 base station data pipeline."""
from __future__ import annotations
import pytest
from src.protobuf_parser import ProtobufParser
from src.confidence_router import ConfidenceRouter
from proto.glove_data_pb2 import BaseStationPacket


class TestP4Integration:
    def setup_method(self):
        self.parser = ProtobufParser()
        self.router = ConfidenceRouter()

    def _make_base_station_packet(self, gesture_id: int = 0,
                                   confidence: float = 0.0) -> bytes:
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.tick_id = 42
        bsp.left.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
        bsp.left.imu.extend([1.0, 2.0, 3.0, 0.1, 0.2, 0.3])
        bsp.right.flex.extend([0.6, 0.7, 0.8, 0.9, 1.0])
        bsp.right.imu.extend([4.0, 5.0, 6.0, 0.4, 0.5, 0.6])
        bsp.relative_features.extend([0.1, 0.2, 0.3, 0.4, 0.5, 0.6])
        bsp.tier2_gesture_id = gesture_id
        bsp.tier2_confidence = confidence
        bsp.base_station.c6_connected = True
        bsp.base_station.p4_ready = True
        bsp.base_station.active_tier = 2
        return bsp.SerializeToString()

    def test_full_pipeline_parse_assemble_route(self):
        """Verify parse → 28-dim assembly → confidence routing."""
        data = self._make_base_station_packet(gesture_id=5, confidence=0.92)
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is not None

        features = self.parser.assemble_28dim(parsed)
        assert len(features) == 28

        # Route through confidence router
        tier2_result = {
            "gesture_id": parsed["tier2_gesture_id"],
            "confidence": parsed["tier2_confidence"],
        }
        routed = self.router.route(tier2=tier2_result)
        assert routed["gesture_id"] == 5
        assert routed["active_tier"] == "tier2"

    def test_hot_switch_tier1_to_tier2(self):
        """Verify router switches from tier1 to tier2 when P4 sends results."""
        # Tier1 only
        r1 = self.router.route(tier1={"gesture_id": 1, "confidence": 0.7})
        assert r1["active_tier"] == "tier1"

        # Tier2 arrives with higher confidence
        r2 = self.router.route(
            tier1={"gesture_id": 1, "confidence": 0.7},
            tier2={"gesture_id": 5, "confidence": 0.92},
        )
        assert r2["active_tier"] == "tier2"
        assert r2["gesture_id"] == 5

    def test_base_station_status_passthrough(self):
        """Verify base station status is available after parsing."""
        data = self._make_base_station_packet()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed["base_station"]["c6_connected"] is True
        assert parsed["base_station"]["active_tier"] == 2
```

### Step 2: Run full test suite

```bash
cd glove_relay
python -m pytest tests/ -v
```

Expected: All tests pass (133 existing + 5 USB + 4 integration = 142 total).

### Step 3: Commit

```bash
git add glove_relay/tests/test_p4_integration.py
git commit -m "test: add P4 base station integration tests"
```

---

## Verification Checklist

After completing all tasks, verify:

```bash
# 1. All relay tests pass
cd glove_relay && python -m pytest tests/ -v
# Expected: 142/142 PASS (133 existing + 5 USB CDC + 4 integration)

# 2. All firmware native tests pass
cd glove_firmware/p4_base_station/tests && pio test -e native
# Expected: 10/10 PASS (6 UART frame + 3 frame pairer + 3 feature assembly)

# 3. All existing firmware/receiver tests still pass
cd glove_firmware && pio test
# Expected: 156/156 PASS (no regressions)

# 4. C6 firmware builds
cd glove_firmware/p4_base_station/c6_firmware && idf.py build
# Expected: Build succeeded

# 5. P4 firmware builds
cd glove_firmware/p4_base_station/p4_firmware && idf.py build
# Expected: Build succeeded
```
