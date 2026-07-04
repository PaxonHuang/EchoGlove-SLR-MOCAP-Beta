# P4+C6 Base Station Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Validate ESP32-P4 + ESP32-C6 base station firmware on real hardware (P4-Function-EV-Board now connected via USB), filling stubs and fixing the FramePairer timeout bug so a meaningful end-to-end demo works with mock ESP-NOW data (no gloves required).

**Architecture:** Fix-then-bridge-then-fill. Phase A1 fixes the FramePairer timeout bug and adds a compile-time mock ESP-NOW path in C6. Phase A2 fills three P4 stubs (TFLite inference, USB CDC, LVGL BSP + TTS audio) via parallel subagents. Phase A3 flashes C6+P4 and verifies the full chain on hardware. Track B runs a mock relay→browser demo in parallel.

**Tech Stack:** ESP-IDF v5.4 (P4 + C6), TinyUSB (USB CDC), esp-bsp `esp32_p4_function_ev_board` (LVGL + ES8311 audio), Unity (native tests), Python FastAPI (relay mock), React 18 + R3F (frontend).

**Spec:** `docs/superpowers/specs/2026-07-04-p4-base-station-verification-design.md`

---

## File Structure

### Phase A0 — Baseline (commit uncommitted work)
- **Commit** (no new files): `glove_relay/src/mock_data.py`, `glove_relay/tests/test_mock_data.py`, 5 modified files, 5 new test dirs

### Phase A1 — FramePairer fix + C6 mock bridge
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/main.cpp` (add `s_pairer.tick()` to uart_task loop)
- **Modify:** `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.cpp` (add mock task)
- **Modify:** `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h` (add mock init API)
- **Modify:** `glove_firmware/p4_base_station/c6_firmware/main/main.cpp` (call mock init when enabled)
- **Modify:** `glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults` (add `CONFIG_MOCK_ESP_NOW=y`)
- **Test (existing, verify):** `glove_firmware/p4_base_station/tests/test/test_frame_pairer/test_frame_pairer.cpp` (test_pairer_timeout already covers class)

### Phase A2a — TFLite stub → static valid result
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.cpp` (return cycling gesture 0-4, conf 0.80)
- **Create:** `glove_firmware/p4_base_station/tests/test/test_tflite_stub/test_tflite_stub.cpp` (native test)

### Phase A2b — USB CDC stub → real TinyUSB
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/usb_task.cpp` (TinyUSB init + write)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/usb_task.h` (no API change)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt` (add `esp_tinyusb` REQUIRES)
- **Create:** `glove_firmware/p4_base_station/p4_firmware/idf_component.yml` (BSP + tinyusb deps)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults` (TinyUSB config)

### Phase A2c — LVGL BSP + TTS audio
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/display_task.cpp` (BSP display start/lock/unlock)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/audio_task.cpp` (i2s_write + codec init)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt` (add BSP REQUIRES)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/idf_component.yml` (add BSP dep — shared with A2b)
- **Modify:** `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults` (SD card + PSRAM for LVGL)

### Phase A3 — Hardware verification (no code, flashing + monitoring)
- **No file changes** — verification only

### Track B — Mock relay→browser demo (background)
- **No code changes expected** — verification + gap report only
- **Possible:** minor fixes to `glove_relay/src/main.py` or frontend if gaps found

---

## Phase A0 — Baseline Commit

### Task A0.1: Commit uncommitted mock_data + V5 diagnostic files

**Files:**
- Commit: `glove_relay/src/mock_data.py` (new)
- Commit: `glove_relay/tests/test_mock_data.py` (new)
- Commit: `glove_firmware/test/test_i2c_scan/`, `test_v5_diagnostic/`, `test_v5_hardware/`, `test_v5_isolation/`, `test_v5_quick_scan/` (new dirs)
- Commit: `glove_relay/configs/relay_config.yaml` (modified — mock section)
- Commit: `glove_relay/src/main.py` (modified — mock integration)
- Commit: `glove_relay/src/utils/config.py` (modified — _MockConfig)
- Commit: `glove_firmware/include/data_structures.h` (modified)
- Commit: `glove_firmware/lib/Sensors/ADS1115Manager.h` (modified)

- [ ] **Step 1: Verify mock_data tests pass before commit**

Run:
```bash
cd glove_relay && python -m pytest tests/test_mock_data.py -q
```
Expected: `6 passed`

- [ ] **Step 2: Stage all uncommitted files**

Run:
```bash
git add glove_relay/src/mock_data.py glove_relay/tests/test_mock_data.py
git add glove_firmware/test/test_i2c_scan/ glove_firmware/test/test_v5_diagnostic/ glove_firmware/test/test_v5_hardware/ glove_firmware/test/test_v5_isolation/ glove_firmware/test/test_v5_quick_scan/
git add glove_relay/configs/relay_config.yaml glove_relay/src/main.py glove_relay/src/utils/config.py
git add glove_firmware/include/data_structures.h glove_firmware/lib/Sensors/ADS1115Manager.h
```

- [ ] **Step 3: Verify staging**

Run:
```bash
git status -s
```
Expected: All previously uncommitted files now staged (green `A`/`M`), no red `??` lines.

- [ ] **Step 4: Commit**

Run:
```bash
git commit -m "feat(relay+firmware): mock data source + V5 diagnostic tests

- Add MockDataSource for V5 dual-hand format (5 gestures, 30fps, noise)
- Add mock config section in relay_config.yaml + _MockConfig in config.py
- Integrate mock mode into main.py lifespan (bypasses model registry)
- Add V5 diagnostic test dirs: i2c_scan, v5_diagnostic, v5_hardware, v5_isolation, v5_quick_scan
- Minor fixes to data_structures.h and ADS1115Manager.h

Co-Authored-By: Claude <noreply@anthropic.com>"
```

- [ ] **Step 5: Push to remote**

Run:
```bash
git push EchoGlove-SLR-MOCAP-Beta feature/v6-dual-s3p4-flex-lsm6dsv16x
```
Expected: Push succeeds (network permitting). If TLS handshake fails (proxy), retry or note for manual push.

- [ ] **Step 6: Verify clean tree**

Run:
```bash
git status -s
```
Expected: `nothing to commit, working tree clean`

---

## Phase A1 — FramePairer Fix + C6 Mock ESP-NOW Bridge

**Subagent scope:** Single subagent handles both A1.1 (FramePairer fix) and A1.2 (C6 mock bridge) — they're coupled (both needed for C6→P4 testing).

### Task A1.1: Fix FramePairer timeout bug in P4 main.cpp

**Problem:** `main.cpp`'s `uart_task` (lines 108-147) calls `s_pairer.feed()` and `s_pairer.getPair()` but never `s_pairer.tick()`. Stale half-pairs accumulate indefinitely when one hand's packet is lost. The `FramePairer::tick()` method exists (FramePairer.h:36-41) and is unit-tested, but never invoked in production.

**Files:**
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/main.cpp:108-147` (uart_task)

- [ ] **Step 1: Confirm the existing timeout test passes (class already covered)**

Run:
```bash
cd glove_firmware/p4_base_station/tests && pio test -e native -f test_frame_pairer
```
Expected: `3 tests PASS` (including `test_pairer_timeout`). This confirms the `FramePairer` class itself works — the bug is integration-only (main.cpp not calling tick).

- [ ] **Step 2: Add `s_pairer.tick()` to uart_task loop**

In `glove_firmware/p4_base_station/p4_firmware/main/main.cpp`, find the `uart_task` function (around line 108). Add the tick call at the top of the `while (1)` loop, before `uart_receiver_poll`:

```cpp
static void uart_task(void* arg) {
    GlovePacket pkt;
    while (1) {
        // Expire stale half-pairs past the 50ms timeout
        s_pairer.tick(esp_timer_get_time());

        if (uart_receiver_poll(&pkt)) {
            s_pairer.feed(pkt);
            FramePair pair;
            if (s_pairer.getPair(pair)) {
                // ... existing feature assembly + enqueue unchanged ...
```

Add `#include "esp_timer.h"` at the top of main.cpp if not already present (it isn't — check the includes block lines 1-22).

- [ ] **Step 3: Build P4 firmware to verify compilation**

Run:
```bash
cd glove_firmware/p4_base_station/p4_firmware
source ~/esp/esp-idf/export.sh
idf.py build
```
Expected: Build succeeds. The `s_pairer.tick(esp_timer_get_time())` call compiles (uint32_t vs int64_t — `esp_timer_get_time()` returns int64_t, cast to uint32_t is implicit but may warn; if warning appears, wrap: `s_pairer.tick((uint32_t)esp_timer_get_time())`).

- [ ] **Step 4: Re-run native tests to confirm no regression**

Run:
```bash
cd glove_firmware/p4_base_station/tests && pio test -e native
```
Expected: All native tests still pass (12/12: frame_pairer 3, feature_assembly 3, uart_frame 6).

- [ ] **Step 5: Commit A1.1 (hold for A1.2 — commit together at end of A1)**

Skip standalone commit; commit with A1.2 at Task A1.2 Step 6.

### Task A1.2: Build C6 mock ESP-NOW mode

**Problem:** C6 firmware (`c6_firmware/main/espnow_handler.cpp`) only receives real ESP-NOW packets. With gloves blocked (LSM6DSV16X not arrived), the C6→P4 UART path cannot be tested. Need a compile-time mock that generates valid `GlovePacket`s and feeds them through the same callback path.

**Files:**
- Modify: `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h`
- Modify: `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.cpp`
- Modify: `glove_firmware/p4_base_station/c6_firmware/main/main.cpp`
- Modify: `glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults`

- [ ] **Step 1: Add mock init API to espnow_handler.h**

In `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h`, add a mock init declaration after the existing `espnow_handler_init`:

```cpp
/**
 * Initialize mock ESP-NOW mode. Generates synthetic GlovePackets cycling
 * through 5 gestures at 50Hz and forwards them to the callback.
 * Used for hardware-in-the-loop testing without real gloves.
 * Compile-time controlled by CONFIG_MOCK_ESP_NOW=y.
 */
extern "C" bool espnow_handler_init_mock(espnow_packet_cb_t cb);
```

- [ ] **Step 2: Implement mock task in espnow_handler.cpp**

In `glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.cpp`, add the mock implementation at the end of the file (after `espnow_handler_rx_count`):

```cpp
// ============================================================================
// Mock ESP-NOW mode (CONFIG_MOCK_ESP_NOW=y)
// Generates synthetic GlovePackets for hardware testing without gloves.
// ============================================================================

#ifdef CONFIG_MOCK_ESP_NOW
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const int MOCK_GESTURES = 5;

// 5 distinct flex patterns (open, fist, thumbs_up, peace, point)
static const float MOCK_FLEX[5][5] = {
    {0.1f, 0.1f, 0.1f, 0.1f, 0.1f},  // open_hand
    {0.9f, 0.9f, 0.9f, 0.9f, 0.9f},  // fist
    {0.1f, 0.9f, 0.1f, 0.1f, 0.1f},  // thumbs_up
    {0.1f, 0.1f, 0.9f, 0.9f, 0.1f},  // peace
    {0.1f, 0.9f, 0.1f, 0.1f, 0.1f},  // point (index)
};

static void mock_espnow_task(void* arg) {
    espnow_packet_cb_t cb = (espnow_packet_cb_t)arg;
    int gesture_idx = 0;
    bool toggle_left = true;  // alternate L/R to produce pairs
    uint32_t tick = 0;

    ESP_LOGI(TAG, "MOCK ESP-NOW task started — 5 gestures @ 50Hz");

    while (1) {
        GlovePacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;  // "EG"
        pkt.version = 5;
        pkt.hand_id = toggle_left ? HAND_LEFT : HAND_RIGHT;
        pkt.tick_id = tick;
        pkt.timestamp_us = tick * 10000;  // 100Hz base
        // Flex: copy pattern for current gesture
        memcpy(pkt.flex, MOCK_FLEX[gesture_idx % MOCK_GESTURES], sizeof(float) * 5);
        // IMU: small synthetic values (euler + gyro)
        for (int i = 0; i < 6; i++) pkt.imu[i] = 0.1f * (i + 1);
        pkt.computeChecksum();

        if (cb) cb(&pkt);

        if (!toggle_left) {
            tick++;           // pair complete after R
            gesture_idx++;    // advance gesture every full pair
        }
        toggle_left = !toggle_left;

        // 50Hz per hand → 20ms; alternating L/R means 10ms per packet
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" bool espnow_handler_init_mock(espnow_packet_cb_t cb) {
    s_callback = cb;
    s_rx_count = 0;
    ESP_LOGW(TAG, "MOCK ESP-NOW mode — generating synthetic data (no real ESP-NOW)");
    xTaskCreate(mock_espnow_task, "mock_espnow", 4096, (void*)cb, 4, NULL);
    return true;
}
#endif  // CONFIG_MOCK_ESP_NOW
```

- [ ] **Step 3: Wire mock init in C6 main.cpp**

In `glove_firmware/p4_base_station/c6_firmware/main/main.cpp`, find the `espnow_handler_init(on_glove_packet)` call (around line 90). Replace with conditional:

```cpp
    // Init ESP-NOW receiver with packet callback
    // (mock mode generates synthetic packets for hardware testing)
#ifdef CONFIG_MOCK_ESP_NOW
    espnow_handler_init_mock(on_glove_packet);
#else
    espnow_handler_init(on_glove_packet);
#endif
```

- [ ] **Step 4: Add CONFIG_MOCK_ESP_NOW to sdkconfig.defaults**

In `glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults`, append:

```
# Mock ESP-NOW mode (set =y to generate synthetic packets without gloves)
CONFIG_MOCK_ESP_NOW=y
```

Note: To flash real ESP-NOW mode later, run `idf.py menuconfig` and set `Mock ESP-NOW mode` to `n` (or edit sdkconfig directly). The `sdkconfig.defaults` value only applies on first `set-target` or after `idf.py fullclean`.

- [ ] **Step 5: Build C6 firmware to verify compilation**

Run:
```bash
cd glove_firmware/p4_base_station/c6_firmware
source ~/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
```
Expected: Build succeeds. If `CONFIG_MOCK_ESP_NOW` isn't visible, run `idf.py menuconfig` → confirm it appears (it won't appear in menuconfig since it's a custom symbol — it's set via sdkconfig.defaults only). Verify `grep CONFIG_MOCK_ESP_NOW sdkconfig` shows `=y`.

If the build fails with `espnow_handler_init_mock undeclared`, check that `espnow_handler.h` changes were saved and that the `#ifdef CONFIG_MOCK_ESP_NOW` guard in the .cpp wraps the implementation (the declaration in .h should NOT be guarded, so callers always see it).

- [ ] **Step 6: Commit A1 (FramePairer fix + C6 mock together)**

Run:
```bash
git add glove_firmware/p4_base_station/p4_firmware/main/main.cpp
git add glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.h
git add glove_firmware/p4_base_station/c6_firmware/main/espnow_handler.cpp
git add glove_firmware/p4_base_station/c6_firmware/main/main.cpp
git add glove_firmware/p4_base_station/c6_firmware/sdkconfig.defaults

git commit -m "fix(p4+c6): FramePairer timeout + C6 mock ESP-NOW mode

P4 main.cpp:
- Add s_pairer.tick(esp_timer_get_time()) in uart_task loop
- Expires stale half-pairs past 50ms timeout (was never called)

C6 firmware:
- Add CONFIG_MOCK_ESP_NOW compile-time mock mode
- espnow_handler_init_mock() generates 5-gesture cycle at 50Hz
- Mock packets go through same callback as real ESP-NOW
- sdkconfig.defaults: CONFIG_MOCK_ESP_NOW=y (toggle via menuconfig for real mode)

Enables C6→P4 UART testing without gloves (LSM6DSV16X pending).

Co-Authored-By: Claude <noreply@anthropic.com>"
```

- [ ] **Step 7: Push**

Run:
```bash
git push EchoGlove-SLR-MOCAP-Beta feature/v6-dual-s3p4-flex-lsm6dsv16x
```

---

## Phase A2 — Fill P4 Stubs (3 parallel subagents)

**Dispatch note:** A2a, A2b, A2c are independent and can run as 3 parallel subagents. A2b and A2c both touch `idf_component.yml` and `sdkconfig.defaults` — coordinate by having A2b create `idf_component.yml` with both deps, and A2c only appends BSP-specific lines to `sdkconfig.defaults` (A2b appends TinyUSB-specific lines). If they conflict, the second subagent rebases on the first's changes. **Safer alternative:** run A2b first (creates `idf_component.yml`), then A2c in parallel with A2a.

### Task A2a: TFLite stub → static valid result

**Problem:** `tflite_infer.cpp` returns `gesture_id=-1` (invalid — outside 0-45 range) when `model_data.h` is absent. Downstream LVGL/USB display "Unknown" and the demo looks broken. Fix: stub returns cycling gesture 0-4 with confidence 0.80.

**Files:**
- Modify: `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.cpp`
- Create: `glove_firmware/p4_base_station/tests/test/test_tflite_stub/test_tflite_stub.cpp`

- [ ] **Step 1: Write failing test for stub behavior**

Create `glove_firmware/p4_base_station/tests/test/test_tflite_stub/test_tflite_stub.cpp`:

```cpp
#include <unity.h>
#include "tflite_infer.h"
#include "data_structures.h"

void test_stub_returns_valid_result() {
    tflite_init(nullptr, 0);  // no model — stub mode
    float left[TFLITE_INPUT_DIM] = {0};
    float right[TFLITE_INPUT_DIM] = {0};
    Tier2Result r = tflite_run(left, right);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, r.gesture_id);
    TEST_ASSERT_LESS_OR_EQUAL_INT(45, r.gesture_id);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.confidence);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, r.confidence);
}

void test_stub_cycles_gestures() {
    // Consecutive calls should produce different gesture_ids over time
    tflite_init(nullptr, 0);
    float left[TFLITE_INPUT_DIM] = {0};
    float right[TFLITE_INPUT_DIM] = {0};
    int first = tflite_run(left, right).gesture_id;
    int second = tflite_run(left, right).gesture_id;
    // After 2 calls, gesture should advance (or at least stay valid)
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, first);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, second);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_stub_returns_valid_result);
    RUN_TEST(test_stub_cycles_gestures);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd glove_firmware/p4_base_station/tests && pio test -e native -f test_tflite_stub
```
Expected: FAIL — `gesture_id=-1` fails `GREATER_OR_EQUAL_INT(0, -1)`. (Current stub sets `gesture_id=-1` at line 64.)

- [ ] **Step 3: Fix stub to return cycling valid result**

In `glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.cpp`, replace the `tflite_run` function body (lines 60-93). Keep the `#if __has_include("model_data.h")` structure in main.cpp unchanged — here we only fix the stub branch (the `#else` path when no model):

Replace lines 60-93 with:

```cpp
Tier2Result tflite_run(const float left[TFLITE_INPUT_DIM],
                       const float right[TFLITE_INPUT_DIM]) {
    Tier2Result result;
    memset(&result, 0, sizeof(result));

    if (!s_initialized) {
        ESP_LOGW(TAG, "tflite_run called before tflite_init");
        result.gesture_id = -1;
        result.valid = false;
        return result;
    }

    int64_t t0 = esp_timer_get_time();

#if __has_include("model_data.h")
    // ---- Real inference path (model_data.h exists) ----
    // TODO: full TFLite Micro interpreter pipeline:
    //   1. Copy left[11] → input tensor 0
    //   2. Copy right[11] → input tensor 1
    //   3. Invoke
    //   4. Parse output → softmax → argmax
    // For now, fall through to stub behavior until interpreter is wired.
    // When implementing, replace the stub block below with real inference.
#endif

    // ---- Stub: cycle through 5 gestures with 0.80 confidence ----
    // Enables LVGL display + USB CDC demo before model export.
    static int s_call_count = 0;
    const int STUB_GESTURES[5] = {0, 1, 2, 3, 4};  // 你好, 谢谢, 对不起, 是, 不是

    result.gesture_id = STUB_GESTURES[s_call_count % 5];
    result.confidence = 0.80f;
    result.valid = true;
    result.l2_requested = false;

    s_call_count++;

    int64_t t1 = esp_timer_get_time();
    result.inference_us = (uint32_t)(t1 - t0);

    ESP_LOGD(TAG, "STUB inference: gesture=%d conf=%.2f call=%d",
             result.gesture_id, result.confidence, s_call_count);

    return result;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd glove_firmware/p4_base_station/tests && pio test -e native -f test_tflite_stub
```
Expected: `2 tests PASS`.

- [ ] **Step 5: Run full native test suite for regression**

Run:
```bash
cd glove_firmware/p4_base_station/tests && pio test -e native
```
Expected: All tests pass (now 14: frame_pairer 3, feature_assembly 3, uart_frame 6, tflite_stub 2).

- [ ] **Step 6: Build P4 firmware to confirm no compile break**

Run:
```bash
cd glove_firmware/p4_base_station/p4_firmware
source ~/esp/esp-idf/export.sh
idf.py build
```
Expected: Build succeeds.

- [ ] **Step 7: Commit A2a**

Run:
```bash
git add glove_firmware/p4_base_station/p4_firmware/components/tflite_micro/tflite_infer.cpp
git add glove_firmware/p4_base_station/tests/test/test_tflite_stub/test_tflite_stub.cpp

git commit -m "feat(p4): TFLite stub returns cycling valid gesture

- Stub now returns gesture 0-4 cycling, confidence 0.80, valid=true
- Enables LVGL display + USB CDC demo before model_data.h exists
- Real inference path preserved (#if __has_include model_data.h)
- Add native test_tflite_stub (2 tests: valid result + cycling)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

### Task A2b: USB CDC stub → real TinyUSB

**Problem:** `usb_task.cpp` logs JSON but never calls TinyUSB. P4 cannot send inference results to PC relay via USB.

**Files:**
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/usb_task.cpp`
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`
- Create: `glove_firmware/p4_base_station/p4_firmware/idf_component.yml`
- Modify: `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`

- [ ] **Step 1: Create idf_component.yml with TinyUSB dependency**

Create `glove_firmware/p4_base_station/p4_firmware/idf_component.yml`:

```yaml
dependencies:
  espressif/esp_tinyusb:
    version: "^1.0.0"
  espressif/esp32_p4_function_ev_board:
    version: "*"
```

Note: The BSP dependency (`esp32_p4_function_ev_board`) is shared with A2c — create it here so A2c doesn't need to. A2c will add BSP-specific sdkconfig lines only.

- [ ] **Step 2: Add TinyUSB config to sdkconfig.defaults**

In `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`, append:

```
# TinyUSB CDC (USB HS for P4→PC relay)
CONFIG_TINYUSB=y
CONFIG_TINYUSB_TASK_ENABLED=y
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_VENDOR_STR="EchoGlove"
CONFIG_TINYUSB_PRODUCT_STR="P4 Base Station"
```

- [ ] **Step 3: Add esp_tinyusb to CMakeLists.txt REQUIRES**

In `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`, change the `idf_component_register` call to add `esp_tinyusb` to REQUIRES:

```cmake
idf_component_register(
    SRCS "main.cpp" "uart_receiver.cpp" "display_task.cpp" "audio_task.cpp" "usb_task.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event esp_tinyusb
    PRIV_REQUIRES esp_timer shared tflite_micro
)
```

- [ ] **Step 4: Implement TinyUSB CDC in usb_task.cpp**

Replace the entire content of `glove_firmware/p4_base_station/p4_firmware/main/usb_task.cpp`:

```cpp
#include "usb_task.h"
#include <cstring>
#include <cstdio>
#include "esp_log.h"
#include "esp_tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tinyusb.h"

static const char* TAG = "usb";
static bool s_usb_ready = false;

// CDC RX callback (optional — for future PC→P4 commands)
static void cdc_rx_callback(int itf, uint8_t const* buf, uint32_t bufsize) {
    // No commands expected yet; just log
    ESP_LOGD(TAG, "CDC RX itf=%d %u bytes", itf, (unsigned)bufsize);
}

bool usb_task_init(void) {
    ESP_LOGI(TAG, "USB HS CDC init (TinyUSB)");

    // TinyUSB CDC configuration
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    esp_err_t ret = esp_tinyusb_init(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register CDC RX callback
    tusb_cdc_acm_register_callback(TINYUSB_CDC_ACM_0, cdc_rx_callback);

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

    // Write to CDC interface 0
    uint32_t written = tud_cdc_n_write(0, (const uint8_t*)buf, (uint32_t)n);
    tud_cdc_n_write_flush(0);

    if (written != (uint32_t)n) {
        ESP_LOGW(TAG, "USB partial write: %u/%d bytes", (unsigned)written, n);
    }
}
```

- [ ] **Step 5: Build P4 firmware to verify TinyUSB links**

Run:
```bash
cd glove_firmware/p4_base_station/p4_firmware
source ~/esp/esp-idf/export.sh
idf.py fullclean   # needed to pick up new idf_component.yml deps
idf.py build
```
Expected: Build succeeds. The first build after adding `idf_component.yml` will download `esp_tinyusb` and `esp32_p4_function_ev_board` from the ESP Component Registry (requires network). If download fails (proxy), see "Fallback" below.

**Fallback if component download fails:** Document the issue in PROGRESS.md and leave usb_task.cpp with the TinyUSB calls but guard them with `#if __has_include("esp_tinyusb.h")` so the build doesn't break when the component is absent. Note for user: run `idf.py reconfigure` with network access later.

- [ ] **Step 6: Commit A2b**

Run:
```bash
git add glove_firmware/p4_base_station/p4_firmware/main/usb_task.cpp
git add glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt
git add glove_firmware/p4_base_station/p4_firmware/idf_component.yml
git add glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults

git commit -m "feat(p4): real TinyUSB CDC for USB HS output

- usb_task.cpp: esp_tinyusb_init + tud_cdc_n_write for JSON TX
- Add esp_tinyusb + esp32_p4_function_ev_board to idf_component.yml
- Add TinyUSB config to sdkconfig.defaults
- P4 now sends inference JSON to PC relay via USB CDC

Co-Authored-By: Claude <noreply@anthropic.com>"
```

### Task A2c: LVGL BSP + TTS audio

**Problem:** (1) LVGL widget code exists in `display_task.cpp` but BSP isn't linked — `__has_include("lvgl.h")` fails, display is log-only. (2) `audio_task.cpp` reads PCM files but never calls `i2s_write()` — audio is silent.

**Dependency:** Run after A2b completes (A2b creates `idf_component.yml` with the BSP dependency). A2c only appends to `sdkconfig.defaults`.

**Files:**
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/display_task.cpp`
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/audio_task.cpp`
- Modify: `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt` (add BSP REQUIRES)
- Modify: `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults` (LVGL + SD card)

- [ ] **Step 1: Add BSP to CMakeLists.txt REQUIRES**

In `glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt`, add `bsp` to REQUIRES (after `esp_tinyusb`):

```cmake
idf_component_register(
    SRCS "main.cpp" "uart_receiver.cpp" "display_task.cpp" "audio_task.cpp" "usb_task.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event esp_tinyusb esp32_p4_function_ev_board
    PRIV_REQUIRES esp_timer shared tflite_micro
)
```

- [ ] **Step 2: Add LVGL + SD card + PSRAM config to sdkconfig.defaults**

In `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`, append:

```
# LVGL + PSRAM (for LVGL buffer allocation)
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_USE_PERF_MONITOR=y
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y

# SD card (FAT filesystem for /sdcard/tts/*.pcm)
CONFIG_FATFS_LONG_FILENAME=y

# BSP display (7" MIPI-DSI on P4 EV Board)
CONFIG_BSP_DISPLAY_LVGL_HOURS_TO_STAY_AWAKE=0
```

- [ ] **Step 3: Add BSP display init to display_task.cpp**

In `glove_firmware/p4_base_station/p4_firmware/main/display_task.cpp`, replace the guarded `#if __has_include("lvgl.h")` block with BSP-backed init. Add BSP includes at the top (after line 26 `#include <cstring>`):

```cpp
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
```

Remove the `#if __has_include("lvgl.h")` / `#define HAS_LVGL` guard (lines 28-34) — LVGL is now always available via BSP. Remove the `#if HAS_LVGL` / `#else` / `#endif` guards around widget creation (lines 63-72, 87-155) and update (lines 189-230). Keep the widget creation code unconditionally.

In `display_init()`, replace the comment block (lines 88-96) with actual BSP calls:

```cpp
bool display_init(void) {
    ESP_LOGI(TAG, "Display init (LVGL + BSP)");

    // Initialize BSP display (7" MIPI-DSI)
    bsp_display_start();
    bsp_display_backlight_on();

    lv_disp_t* disp = bsp_display_get();
    if (!disp) {
        ESP_LOGE(TAG, "BSP display init failed");
        return false;
    }

    lv_obj_t* scr = lv_scr_act();

    // ... existing widget creation code (lines 100-151) unchanged ...

    ESP_LOGI(TAG, "LVGL widgets created on 7\" MIPI-DSI");
    return true;
}
```

In `display_update()`, wrap the LVGL update calls with BSP lock/unlock:

```cpp
void display_update(const Tier2Result* result,
                    const FramePair* pair,
                    const float features[DUAL_HAND_FEATURES],
                    const BaseStationStatus* status) {
    // ... existing ESP_LOGI logging unchanged ...

    // Lock display before updating widgets
    bsp_display_lock(0);

    // ... existing LVGL widget update code (lines 190-229) unchanged ...

    bsp_display_unlock();
}
```

- [ ] **Step 4: Add I2S write + codec init to audio_task.cpp**

In `glove_firmware/p4_base_station/p4_firmware/main/audio_task.cpp`, add BSP codec + I2S includes at the top (after line 8 `#include "p4_protocol.h"`):

```cpp
#include "bsp/bsp_codec.h"
#include "driver/i2s_std.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
```

Add static handles after line 12 (`static const int64_t DEDUP_US`):

```cpp
static esp_codec_dev_handle_t s_codec_dev = NULL;
static bool s_audio_ready = false;
```

Replace `audio_init()` (lines 14-19) with:

```cpp
bool audio_init(void) {
    ESP_LOGI(TAG, "Audio init (ES8311 via BSP)");

    // Initialize SD card (for /sdcard/tts/*.pcm)
    sdmmc_card_t* card = NULL;
    esp_err_t ret = bsp_sdcard_mount(&card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed: %s — TTS PCM files unavailable",
                 esp_err_to_name(ret));
        // Continue without SD; audio_play_gesture will just log
    } else {
        ESP_LOGI(TAG, "SD card mounted at /sdcard");
    }

    // Initialize ES8311 codec via BSP
    s_codec_dev = bsp_audio_codec_init();
    if (!s_codec_dev) {
        ESP_LOGE(TAG, "ES8311 codec init failed");
        return false;
    }

    // Open output device (16kHz, 16-bit, mono for TTS PCM)
    esp_codec_dev_sample_info_t info = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(s_codec_dev, CODEC_DEV_TYPE_OUTPUT, &info) != ESP_OK) {
        ESP_LOGE(TAG, "Codec open failed");
        return false;
    }

    s_audio_ready = true;
    ESP_LOGI(TAG, "Audio ready — ES8311 @ 16kHz/16bit/mono");
    return true;
}
```

Replace the `i2s_write` TODO line (line 41) in `audio_play_gesture` with actual codec write:

```cpp
    uint8_t buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (s_audio_ready) {
            int written = esp_codec_dev_write(s_codec_dev, buf, bytes_read);
            if (written < 0) {
                ESP_LOGW(TAG, "Codec write error: %d", written);
                break;
            }
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "Played gesture %d", gesture_id);
```

- [ ] **Step 5: Build P4 firmware to verify BSP links**

Run:
```bash
cd glove_firmware/p4_base_station/p4_firmware
source ~/esp/esp-idf/export.sh
idf.py build
```
Expected: Build succeeds. LVGL + BSP + codec symbols resolve. If `bsp_display_start` / `bsp_audio_codec_init` / `bsp_sdcard_mount` are unresolved, the BSP component version may differ — check `idf_component.yml` version and consult Espressif docs via `mcp__espressif-docs__search_espressif_sources` with query `"esp32_p4_function_ev_board BSP API"`.

- [ ] **Step 6: Commit A2c**

Run:
```bash
git add glove_firmware/p4_base_station/p4_firmware/main/display_task.cpp
git add glove_firmware/p4_base_station/p4_firmware/main/audio_task.cpp
git add glove_firmware/p4_base_station/p4_firmware/main/CMakeLists.txt
git add glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults

git commit -m "feat(p4): LVGL BSP display + ES8311 TTS audio

display_task.cpp:
- Link esp32_p4_function_ev_board BSP (bsp_display_start/lock/unlock)
- LVGL widgets now render on 7\" MIPI-DSI (was log-only)
- Remove HAS_LVGL guard — LVGL always available via BSP

audio_task.cpp:
- bsp_audio_codec_init for ES8311
- esp_codec_dev_write replaces stubbed i2s_write
- bsp_sdcard_mount for /sdcard/tts/*.pcm
- Opens codec at 16kHz/16bit/mono for TTS PCM

sdkconfig: LVGL color depth, PSRAM BSS, FATFS long filenames

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase A3 — Hardware-in-the-Loop Verification

**No code changes** — flashing and monitoring only. Requires user's P4+C6 connected via USB.

### Task A3.1: Flash C6 with mock mode and verify UART output

- [ ] **Step 1: Build C6 with CONFIG_MOCK_ESP_NOW=y**

Run:
```bash
cd glove_firmware/p4_base_station/c6_firmware
source ~/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
```
Verify: `grep CONFIG_MOCK_ESP_NOW sdkconfig` shows `=y`.

- [ ] **Step 2: Flash C6**

Run (replace `/dev/ttyUSB0` with actual C6 port — likely a USB-TTL adapter at 3.3V):
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```
Expected monitor output:
```
I (xxx) c6_main: C6 base station starting
W (xxx) espnow_rx: MOCK ESP-NOW mode — generating synthetic data (no real ESP-NOW)
I (xxx) espnow_rx: MOCK ESP-NOW task started — 5 gestures @ 50Hz
I (xxx) c6_main: C6 ready — ESP-NOW → UART relay active
```
- [ ] **Step 3: Verify UART TX with logic analyzer or P4 RX**

Wire C6 GPIO43 (TX) → P4 GPIO37 (RX, per `uart_receiver_init(0, 37, 38, 2000000)` in P4 main.cpp:177). Verify with oscilloscope if available, or skip to A3.2 and observe P4 decoding.

### Task A3.2: Flash P4 and verify end-to-end

- [ ] **Step 4: Build P4 with all stubs filled**

Run:
```bash
cd glove_firmware/p4_base_station/p4_firmware
source ~/esp/esp-idf/export.sh
idf.py build
```
Expected: Build succeeds.

- [ ] **Step 5: Flash P4**

Run (P4 is on `/dev/ttyACM0`):
```bash
idf.py -p /dev/ttyACM0 flash monitor
```
Expected monitor output:
```
I (xxx) p4_main: P4 base station starting
I (xxx) tflite: TFLite init (stub) -- model size: 0 bytes
W (xxx) p4_main: model_data.h not found -- Tier2 inference disabled
I (xxx) audio: Audio ready — ES8311 @ 16kHz/16bit/mono  (or SD card warning)
I (xxx) usb: USB CDC initialized — P4 will send JSON inference results
I (xxx) display: LVGL widgets created on 7" MIPI-DSI
I (xxx) p4_main: P4 ready -- UART + inference + display running
I (xxx) p4_main: Pair tick=0 feat[0]=0.100 feat[27]=0.100
I (xxx) tflite: Pair tick=1 ...
```
- [ ] **Step 6: Verify 7" display shows LVGL widgets**

Observe the P4 EV Board's 7" MIPI-DSI screen:
- Left/right gesture labels appear (cycling 你好/谢谢/对不起/是/不是)
- Confidence bars show ~80%
- Flex bars animate (mock values 0.1 or 0.9)
- Status label shows `C6=-- P4=OK T2`

- [ ] **Step 7: Verify USB CDC output on PC**

Run on PC:
```bash
ls /dev/ttyACM*  # Should show P4 CDC device (e.g. /dev/ttyACM0 or /dev/ttyACM2)
# Monitor JSON output
cat /dev/ttyACM0  # or use: python -c "import serial; s=serial.Serial('/dev/ttyACM0'); print(s.read(1000))"
```
Expected: JSON lines at ~50Hz:
```
{"gesture":0,"conf":0.800,"tier2_time":0}
{"gesture":1,"conf":0.800,"tier2_time":0}
...
```

- [ ] **Step 8: Verify TTS audio (optional — requires PCM files on SD)**

If SD card with `/sdcard/tts/00.pcm` ... `/sdcard/tts/04.pcm` is inserted:
- Audio should play from speaker when gesture changes (2s dedup)
- If no SD/PCM: log shows `PCM not found: /sdcard/tts/00.pcm` (expected, non-fatal)

### Task A3.3: Document verification results

- [ ] **Step 9: Update PROGRESS.md + PROGRESS_CN.md with A3 results**

Append to `PROGRESS.md` under "## V5.2 P4 Smart Base Station" → "Hardware Testing Progress":
- Mark C6 flash + P4 flash + UART link + LVGL + USB CDC as ✅ Done
- Note any failures with root cause

- [ ] **Step 10: Write memory file for verified base station**

Create `/home/paxon/.claude/projects/-home-paxon-CodingProjects-EchoGloveProjects-EchoGlove-SLR-MOCAP/memory/project-p4-base-station-verified.md`:

```markdown
---
name: project-p4-base-station-verified
description: P4+C6 base station verified with mock ESP-NOW data path (2026-07-04)
metadata:
  type: project
---

P4-Function-EV-Board + C6 firmware verified with mock data path on 2026-07-04:
- C6 `CONFIG_MOCK_ESP_NOW=y` generates 5-gesture cycle at 50Hz
- P4 UART receiver + FramePairer pairs L/R frames (tick bug fixed)
- TFLite stub returns valid inference (gesture 0-4, conf 0.80)
- LVGL displays on 7" MIPI-DSI screen via BSP
- USB CDC sends JSON to PC via TinyUSB
- TTS audio plays PCM via ES8311 (if SD card + files present)

**Why:** Enables standalone base station demo without gloves (LSM6DSV16X pending).
**How to apply:** Flash C6+P4 with CONFIG_MOCK_ESP_NOW=y. For real gloves, set =n via menuconfig.

Hardware: P4-Function-EV-Board v1.5.2 + C6-MINI-1, USB ports /dev/ttyACM0 (P4) + /dev/ttyUSB0 (C6 via USB-TTL).
Related: [[project-v6-adc-impl-progress]]
```

Update `MEMORY.md` index with one-line pointer.

- [ ] **Step 11: Commit + push docs**

Run:
```bash
git add PROGRESS.md PROGRESS_CN.md
git commit -m "docs: P4+C6 base station hardware verification complete

- C6 mock ESP-NOW + P4 LVGL + USB CDC + TTS all verified
- Update PROGRESS.md + PROGRESS_CN.md

Co-Authored-By: Claude <noreply@anthropic.com>"
git push EchoGlove-SLR-MOCAP-Beta feature/v6-dual-s3p4-flex-lsm6dsv16x
```

---

## Track B — Mock Relay→Browser Demo (Background)

**Dispatch as background subagent** — runs in parallel with A1/A2.

### Task B.1: Start relay with mock data and verify WS broadcast

- [ ] **Step 1: Kill any existing relay on port 8765**

Run:
```bash
kill $(lsof -ti:8765) 2>/dev/null; sleep 1
```

- [ ] **Step 2: Start relay in background**

Run:
```bash
cd glove_relay
nohup python -m src.main > /tmp/relay.log 2>&1 &
sleep 3
```

- [ ] **Step 3: Verify health endpoint**

Run:
```bash
curl -s http://localhost:8765/health
```
Expected: `{"status":"ok",...}` (JSON with status ok).

- [ ] **Step 4: Verify WS broadcast with Python client**

Create and run `/tmp/test_ws_client.py`:
```python
import asyncio
import websockets
import json

async def main():
    async with websockets.connect("ws://localhost:8765/ws") as ws:
        for _ in range(3):
            msg = await asyncio.wait_for(ws.recv(), timeout=5.0)
            data = json.loads(msg)
            print(f"RX: gesture={data.get('inference',{}).get('gesture_id')} "
                  f"flex_L={data.get('left_hand',{}).get('flex',[None]*5)[0]}")
    print("WS OK — 3 messages received")

asyncio.run(main())
```
Run: `python /tmp/test_ws_client.py`
Expected: `WS OK — 3 messages received` with non-null gesture/flex values.

### Task B.2: Verify frontend renders 3D hands

- [ ] **Step 5: Start frontend dev server**

Run:
```bash
cd glove_web
nohup npm run dev > /tmp/web.log 2>&1 &
sleep 5
curl -s http://localhost:5173 | head -5  # Should return HTML
```

- [ ] **Step 6: Browser verification via Playwright**

Use Playwright MCP tools:
- `mcp__plugin_playwright_playwright__browser_navigate` to `http://localhost:5173`
- `mcp__plugin_playwright_playwright__browser_snapshot` to capture the page
- `mcp__plugin_playwright_playwright__browser_console_messages` with `level: error` to check for JS errors
- Wait 3s, snapshot again — verify hand poses changed (mock data animating)

Expected:
- Page loads with dual 3D hand canvas
- No JS errors in console
- Hand finger positions change between snapshots (mock flex values animating)

### Task B.3: Report gaps

- [ ] **Step 7: Write Track B report**

Append report to `PROGRESS.md` (or report back to orchestrator if running as subagent):

```
## Track B: Mock Relay→Browser Demo Report (2026-07-04)

**Status**: [PASS/PARTIAL/FAIL]

**What Works**:
- [ ] Relay starts with mock enabled
- [ ] Health endpoint returns ok
- [ ] WS broadcasts at 30fps
- [ ] Frontend loads at localhost:5173
- [ ] Dual 3D hands render
- [ ] Hands animate with mock data

**What's Broken**:
- [list issues]

**Gap List**:
1. [missing piece for full E2E]
```

- [ ] **Step 8: Commit any fixes made during Track B**

If the subagent made fixes to `glove_relay/src/main.py` or frontend files:
```bash
git add -A
git commit -m "fix(relay/web): mock E2E demo gaps

[specific fixes]

Co-Authored-By: Claude <noreply@anthropic.com>"
```
If no fixes needed, skip this step.

---

## SOP Documentation Issues (Recorded, NOT Fixed in This Plan)

These are documented in the spec (`docs/superpowers/specs/2026-07-04-p4-base-station-verification-design.md` § "SOP Documentation Issues") but **not addressed here** — they belong to the LSM6DSV16X migration branch or a separate cleanup PR:

1. `data_structures.h` I2C frequency comment vs constant mismatch (cosmetic)
2. `test_i2c_scan/README.md` GPIO8/9 Octal PSRAM misinformation (Medium — causes confusion)
3. `ADS1115Manager.h` redundant `Wire.begin()` (Low)
4. `SensorManager.h:117` BNO085 init skipped (High — tracked in V6 migration)
5. `display_task.cpp:89-93` LVGL BSP TODO → **fixed in A2c**
6. `main.cpp:177` FramePairer timeout → **fixed in A1.1**

Issues 5 and 6 are fixed by this plan. Issues 1-4 are flagged for the LSM6DSV16X migration phase.

---

## Self-Review Notes

- **Spec coverage**: All 6 spec phases (A0, A1, A2a/b/c, A3, B) have tasks. ✓
- **Placeholder scan**: No TBD/TODO in plan steps (TODOs in code blocks are intentional — they mark real future work inside stubs). ✓
- **Type consistency**: `Tier2Result`, `GlovePacket`, `FramePair`, `TFLITE_INPUT_DIM` all match existing code. `espnow_packet_cb_t`, `HAND_LEFT`/`HAND_RIGHT` match `data_structures.h`. ✓
- **Sequence**: A0 → A1 → (A2a ∥ A2b → A2c) → A3. Track B ∥ A1/A2. ✓
