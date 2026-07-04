# P4+C6 Base Station Verification Roadmap

**Date**: 2026-07-04
**Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
**Status**: Design approved — pending implementation

---

## Executive Summary

**Goal**: Validate ESP32-P4 + ESP32-C6 base station firmware on real hardware (P4-Function-EV-Board now connected), unblocking standalone Tier2 inference + LVGL display + TTS audio path while gloves await LSM6DSV16X arrival.

**Approach**: Fix critical bugs + fill stubs + build mock data bridge for hardware-in-the-loop testing. Parallel subagents for independent subsystems. Background mock E2E demo (relay → browser) runs concurrently.

**Estimated Duration**: 4-5 hours wall-clock time with parallel subagents (A2a/b/c concurrent; Track B overlaps A1/A2).

---

## Architecture Overview

```
Track A (base station, primary)         Track B (background, parallel)
─────────────────────────────────       ─────────────────────────────────
A0: baseline commit + push              B1: relay mock:true → WS → browser
A1: FramePairer bug fix + C6 mock        B2: verify 3D hands animate
    bridge (subagent)                    B3: report gaps
A2: 3 parallel subagents fill P4 stubs
    ├─ A2a: TFLite stub → static gesture
    ├─ A2b: USB CDC stub → real TinyUSB
    └─ A2c: LVGL BSP + TTS audio (ES8311)
A3: flash C6 + P4, verify E2E with mock packets
A4: commit per task, push, update PROGRESS + memory
```

**Dependencies**:
- A0 runs first (clean baseline)
- A1 and B1 run in parallel after A0
- A2 depends on A1 (mock bridge needed for testing)
- A3 requires hardware (user's P4+C6 connected via USB)
- A4 is continuous (commit after each phase)

---

## Phase A0 — Baseline Commit (10 min, solo)

### Objective
Commit uncommitted work-in-progress files to establish a clean baseline for subagents.

### Files to Commit

**Modified files** (5):
- `glove_firmware/include/data_structures.h` — minor updates
- `glove_firmware/lib/Sensors/ADS1115Manager.h` — minor fix
- `glove_relay/configs/relay_config.yaml` — mock config section
- `glove_relay/src/main.py` — mock mode integration
- `glove_relay/src/utils/config.py` — mock config class

**New files** (7):
- `glove_relay/src/mock_data.py` — V5 dual-hand mock data source (111 lines)
- `glove_relay/tests/test_mock_data.py` — mock data tests (6 tests)
- `glove_firmware/test/test_i2c_scan/` — I2C diagnostic
- `glove_firmware/test/test_v5_diagnostic/` — V5 hardware diagnostic
- `glove_firmware/test/test_v5_hardware/` — hardware tests
- `glove_firmware/test/test_v5_isolation/` — isolation tests
- `glove_firmware/test/test_v5_quick_scan/` — quick I2C scan

### Commit Message
```
feat(relay+firmware): mock data source + V5 diagnostic tests

- Add MockDataSource for V5 dual-hand format (5 gestures, 30fps)
- Add mock config section in relay_config.yaml
- Add V5 diagnostic test directories (I2C scan, hardware, isolation)
- Minor fixes to data_structures.h and ADS1115Manager.h
```

### Verification
```bash
git status  # Should show: "nothing to commit, working tree clean"
git push EchoGlove-SLR-MOCAP-Beta feature/v6-dual-s3p4-flex-lsm6dsv16x
```

---

## Phase A1 — C6 Mock Bridge + FramePairer Fix (1 hour, subagent)

### Objective
Fix critical FramePairer timeout bug and build mock ESP-NOW data injection path in C6 firmware for hardware-in-the-loop testing.

### Subagent Scope
Single subagent handles both tasks (coupled — both needed for C6→P4 testing).

### Task A1.1 — Fix FramePairer Timeout Bug

**Problem**: `glove_firmware/p4_base_station/p4_firmware/main/main.cpp:177` never calls `s_pairer.tick()`, causing 50ms timeout to never trigger. Half-paired frames accumulate indefinitely.

**Solution**:
```cpp
// In main loop (main.cpp:177)
void app_main() {
    // ... existing setup ...
    while (true) {
        // Process UART frames
        uart_rx_task();

        // NEW: Tick the pairer to expire stale half-pairs
        s_pairer.tick(esp_timer_get_time());

        // Check for paired frames
        if (s_pairer.getPair(left, right)) {
            // ... existing inference path ...
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Test**: Add native test in `glove_firmware/p4_base_station/tests/test_frame_pairer.cpp`:
- Given: left frame at t=0
- When: 60ms passes without right frame
- Then: left frame is expired, pair request returns false

### Task A1.2 — Build C6 Mock ESP-NOW Mode

**Problem**: C6 firmware (`c6_firmware/main/espnow_handler.cpp`) only receives real ESP-NOW packets. With gloves blocked, C6→P4 UART path cannot be tested.

**Solution**: Add compile-time mock mode controlled by `CONFIG_MOCK_ESP_NOW=y` in `sdkconfig`:

```cpp
// espnow_handler.cpp
#ifdef CONFIG_MOCK_ESP_NOW
static void mock_espnow_task(void* arg) {
    GlovePacket packet;
    packet.magic = 0x4547;  // "EG"
    packet.version = 5;

    const GesturePattern PATTERNS[5] = {
        {/* open_hand */},
        {/* fist */},
        {/* thumbs_up */},
        {/* peace */},
        {/* point */}
    };

    int gesture_idx = 0;
    while (true) {
        // Cycle through 5 gestures, 3s each
        generate_mock_packet(&packet, &PATTERNS[gesture_idx % 5]);
        packet.tick_id = esp_timer_get_time() / 10000;  // 100Hz

        // Encode to UART frame and queue for TX
        uint8_t frame[73];
        uart_frame_encode(&packet, frame);
        xQueueSend(uart_tx_queue, frame, portMAX_DELAY);

        gesture_idx++;
        vTaskDelay(pdMS_TO_TICKS(1000 / 50));  // 50Hz
    }
}
#endif

// In espnow_handler_init()
#ifdef CONFIG_MOCK_ESP_NOW
    xTaskCreate(mock_espnow_task, "mock_espnow", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "MOCK ESP-NOW mode enabled — generating synthetic data");
#else
    // Real ESP-NOW init
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);
#endif
```

**Verification**:
1. Build C6 with `CONFIG_MOCK_ESP_NOW=y`
2. Flash to C6
3. Monitor UART output — should see 73-byte frames at 50Hz
4. Connect C6 TX → P4 RX (hardware UART pins)
5. P4 should receive and decode mock packets

### Deliverables
- Fixed `main.cpp` with `s_pairer.tick()` call
- New native test for timeout expiration
- `espnow_handler.cpp` with `CONFIG_MOCK_ESP_NOW` path
- `sdkconfig.defaults` with `CONFIG_MOCK_ESP_NOW=y` option
- README update documenting mock mode

### Commit Message
```
fix(p4): FramePairer timeout bug + C6 mock ESP-NOW mode

- Add s_pairer.tick() call in P4 main loop to expire stale half-pairs
- Add native test for 50ms timeout expiration
- Add CONFIG_MOCK_ESP_NOW compile-time mock for C6 firmware
- Mock generates 5-gesture cycle at 50Hz for hardware testing
- Real ESP-NOW path unchanged when CONFIG_MOCK_ESP_NOW=n
```

---

## Phase A2 — Fill P4 Stubs (2-3 hours, 3 parallel subagents)

### Objective
Replace placeholder stubs in P4 firmware with functional implementations. Three independent subagents work in parallel.

### A2a — TFLite Inference Stub → Static Valid Result

**Problem**: `components/tflite_micro/tflite_infer.cpp` returns `gesture=-1, valid=false` when `model_data.h` is missing. This breaks downstream LVGL display and USB CDC output.

**Solution**: Make stub return a valid static result instead:

```cpp
// tflite_infer.cpp
InferenceResult tflite_run(const float features[28]) {
    InferenceResult result;

#if __has_include("model_data.h")
    // Real inference path (when model exists)
    // ... existing TFLite code ...
#else
    // Mock stub: return static valid result
    static int gesture_cycle = 0;
    const int GESTURES[5] = {0, 1, 2, 3, 4};  // 你好, 谢谢, 对不起, 没关系, 再见

    result.gesture_id = GESTURES[gesture_cycle % 5];
    result.confidence = 0.80f;
    result.valid = true;
    result.l2_requested = false;

    gesture_cycle++;

    ESP_LOGI(TAG, "STUB inference: gesture=%d conf=%.2f (no model_data.h)",
             result.gesture_id, result.confidence);
#endif

    return result;
}
```

**Test**: Native test in `tests/test_tflite_stub.cpp`:
- Given: no `model_data.h`
- When: call `tflite_run(features)`
- Then: returns `valid=true`, `gesture_id` in [0-45], `confidence` in [0.0-1.0]

**Deliverables**:
- Modified `tflite_infer.cpp` with static stub
- Native test for stub behavior
- README note: "Stub inference until model export"

### A2b — USB CDC Stub → Real TinyUSB

**Problem**: `main/usb_task.cpp` logs JSON but never calls TinyUSB. P4 cannot send inference results to PC relay via USB.

**Solution**: Implement real TinyUSB CDC:

```cpp
// usb_task.cpp
#include "tusb.h"
#include "esp_tinyusb.h"

static uint8_t cdc_rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];

void usb_init(void) {
    // Initialize TinyUSB
    esp_tinyusb_config_t tusb_cfg = {
        .external_phy = false,  // Use internal USB PHY
    };
    esp_tinyusb_init(&tusb_cfg);

    // Initialize CDC
    tusb_init();

    ESP_LOGI(TAG, "USB CDC initialized");
}

void usb_send_json(const char* json_str) {
    if (!tusb_mounted()) {
        ESP_LOGW(TAG, "USB not mounted, skipping send");
        return;
    }

    // Send via CDC
    uint32_t len = strlen(json_str);
    uint32_t sent = tud_cdc_write(json_str, len);
    tud_cdc_write_flush();

    ESP_LOGD(TAG, "USB sent %u/%u bytes", sent, len);
}

void usb_task(void* arg) {
    usb_init();

    while (true) {
        // Process USB events
        tud_task();

        // Check for incoming data (optional)
        if (tud_cdc_available()) {
            uint32_t count = tud_cdc_read(cdc_rx_buf, sizeof(cdc_rx_buf));
            // Handle incoming commands from PC if needed
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Dependencies**:
- Add `esp_tinyusb` component to `CMakeLists.txt`
- Update `sdkconfig`:
  ```
  CONFIG_TINYUSB=y
  CONFIG_TINYUSB_CDC=y
  CONFIG_TINYUSB_CDC_RX_BUFSIZE=1024
  CONFIG_TINYUSB_CDC_TX_BUFSIZE=1024
  ```

**Test**: Hardware test (cannot be native):
- Flash P4 with USB CDC
- Connect USB cable to PC
- `ls /dev/ttyACM*` should show new device
- `cat /dev/ttyACM0` should show JSON inference results

**Deliverables**:
- Modified `usb_task.cpp` with TinyUSB
- Updated `CMakeLists.txt` with `esp_tinyusb`
- Updated `sdkconfig.defaults` with USB config
- README: "USB CDC requires `pip install pyserial` on relay side"

### A2c — LVGL BSP Integration + TTS Audio

**Problem**:
1. LVGL widgets are coded but BSP not linked — display is log-only
2. TTS reads PCM files but never calls `i2s_write()` — audio is silent

**Solution**: Integrate `esp32_p4_function_ev_board` BSP component.

#### LVGL BSP

```cpp
// display_task.cpp
#include "bsp/display.h"
#include "bsp/esp-bsp.h"

static lv_disp_t* display = NULL;

void display_init(void) {
    // Initialize BSP display
    bsp_display_start();

    // Get display object
    display = bsp_display_get();

    // Create LVGL widgets (existing code)
    create_widgets();

    ESP_LOGI(TAG, "LVGL display initialized on 7\" MIPI-DSI");
}

void display_update(const FramePair& pair, const InferenceResult& result) {
    // Lock display
    bsp_display_lock(0);

    // Update widgets (existing code)
    update_hand_widgets(pair);
    update_inference_widgets(result);

    // Unlock display
    bsp_display_unlock();
}
```

#### TTS Audio

```cpp
// audio_task.cpp
#include "bsp/bsp_codec.h"
#include "driver/i2s_std.h"

static i2s_chan_handle_t i2s_tx_handle = NULL;

void audio_init(void) {
    // Initialize I2S via BSP
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &i2s_tx_handle, NULL);

    // Configure ES8311 codec via BSP
    esp_codec_dev_handle_t codec = bsp_audio_codec_init();
    esp_codec_dev_open(codec, CODEC_DEV_TYPE_OUTPUT);

    ESP_LOGI(TAG, "TTS audio initialized (ES8311 via I2S)");
}

void audio_play_pcm(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "PCM not found: %s", path);
        return;
    }

    uint8_t buf[4096];
    size_t bytes_read;
    size_t bytes_written;

    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        // NEW: Actually write to I2S
        i2s_channel_write(i2s_tx_handle, buf, bytes_read, &bytes_written, portMAX_DELAY);
    }

    fclose(f);
    ESP_LOGI(TAG, "Played PCM: %s", path);
}
```

**Dependencies**:
- Add `esp32_p4_function_ev_board` component to `CMakeLists.txt`
- Update `idf_component.yml`:
  ```yaml
  dependencies:
    esp32_p4_function_ev_board:
      version: "*"
  ```
- Mount SD card for `/sdcard/tts/*.pcm`:
  ```cpp
  esp_vfs_fat_sdmmc_mount("/sdcard", card_handle, ...);
  ```

**Test**: Hardware test (requires P4 EV Board):
- Flash P4
- Verify 7" display shows LVGL widgets
- Verify gesture updates when mock data flows
- Verify audio plays from speaker (if PCM files exist)

**Deliverables**:
- Modified `display_task.cpp` with BSP calls
- Modified `audio_task.cpp` with I2S write
- Updated `CMakeLists.txt` with BSP component
- Updated `idf_component.yml` with BSP dependency
- Updated `sdkconfig.defaults` with SD card mount

### A2 Verification Summary

| Subagent | Scope | Test Type | Duration |
|----------|-------|-----------|----------|
| A2a | TFLite stub | Native | 30 min |
| A2b | USB CDC | Hardware | 45 min |
| A2c | LVGL + TTS | Hardware | 1.5 hours |

**Parallelization**: A2a, A2b, A2c run concurrently. Total wall-clock time ≈ 1.5-2 hours.

---

## Phase A3 — Hardware-in-the-Loop Verification (1 hour, hardware required)

### Objective
Flash C6 + P4 with all fixes, verify end-to-end data flow with mock ESP-NOW packets.

### Prerequisites
- P4-Function-EV-Board connected via USB
- C6 module connected via USB-TTL adapter (3.3V!)
- UART wired: C6 TX (GPIO43) → P4 RX (GPIO44)

### Step-by-Step

#### 3.1 Flash C6 with Mock Mode
```bash
cd glove_firmware/p4_base_station/c6_firmware
idf.py menuconfig  # Set CONFIG_MOCK_ESP_NOW=y
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**Expected output**:
```
I (324) espnow: MOCK ESP-NOW mode enabled — generating synthetic data
I (1024) uart: TX frame: 73 bytes, tick_id=102
```

#### 3.2 Flash P4 with Stubs
```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

**Expected output**:
```
I (524) uart: RX frame decoded: tick_id=102, magic=0x4547
I (528) pairer: Paired L+R at tick_id=102
I (532) tflite: STUB inference: gesture=0 conf=0.80
I (536) display: Updated widgets for gesture 0
I (540) usb: Sent JSON: {"gesture":"你好","confidence":0.80}
```

#### 3.3 Verify P4 USB CDC Output
On PC:
```bash
# Find P4 USB device
ls /dev/ttyACM*  # Should show /dev/ttyACM0 or /dev/ttyACM1

# Monitor output
cat /dev/ttyACM0  # Should see JSON inference results

# Or use screen
screen /dev/ttyACM0 115200
```

#### 3.4 Verify 7" Display
- Should show dual-hand LVGL widgets
- Gesture name should update every 3s (5-gesture cycle)
- Confidence bars should show ~80%
- Flex bars should animate (mock values)

#### 3.5 Verify Audio (Optional)
- Place PCM files on SD card: `/sdcard/tts/00.pcm`, `/sdcard/tts/01.pcm`, ...
- Audio should play when gesture changes (2s dedup window)

### Failure Modes

| Failure | Likely Cause | Debug Path |
|---------|--------------|------------|
| No UART frames at P4 | C6 TX→P4 RX wiring wrong | Oscilloscope on C6 TX pin |
| CRC errors | Baud rate mismatch | Verify 2000000 baud on both ends |
| No USB CDC device | TinyUSB not initialized | Check `sdkconfig` for `CONFIG_TINYUSB=y` |
| No LVGL display | BSP not linked | Check `idf.py menuconfig` → BSP components |
| No audio | ES8311 not initialized | I2C scan for ES8311 at 0x18 |

---

## Phase A4 — Continuous Documentation

### Objective
After each phase (A0/A1/A2a/A2b/A2c/A3), commit changes and update documentation.

### Per-Phase Checklist

**Code**:
- [ ] Build passes: `idf.py build` (P4) and `idf.py build` (C6)
- [ ] Tests pass: Native tests for modified components
- [ ] Commit with descriptive message

**Docs**:
- [ ] Update `PROGRESS.md` with phase status
- [ ] Update `PROGRESS_CN.md` (Chinese version)
- [ ] Update README if user-facing behavior changed
- [ ] Write memory file for critical learnings

**Push**:
- [ ] Push to remote: `git push EchoGlove-SLR-MOCAP-Beta feature/v6-dual-s3p4-flex-lsm6dsv16x`

### Final Memory File

After A3 verification succeeds:
```markdown
---
name: project-p4-base-station-verified
description: P4+C6 base station verified with mock ESP-NOW data
metadata:
  type: project
---

P4-Function-EV-Board + C6 firmware verified with mock data path:
- C6 mock ESP-NOW generates 5-gesture cycle at 50Hz
- P4 UART receiver + FramePairer pairs L/R frames
- TFLite stub returns valid inference (gesture 0-4, conf 0.80)
- LVGL displays on 7" MIPI-DSI screen
- USB CDC sends JSON to PC relay
- TTS audio plays PCM via ES8311 (if files present)

**Why**: Enables standalone base station testing without gloves.
**How to apply**: Flash C6+P4 with CONFIG_MOCK_ESP_NOW=y for demo.

Hardware: P4-Function-EV-Board v1.5.2 + C6-MINI-1
Date: 2026-07-04
```

---

## Track B — Mock E2E Demo (Background, 30 min)

### Objective
Verify relay mock mode → WebSocket → frontend 3D hand rendering in browser.

### Execution (Background Subagent)

```bash
# B1: Start relay with mock enabled
cd glove_relay
python -m src.main

# B2: Verify health
curl http://localhost:8765/health
# Expected: {"status":"ok","mock":true}

# B3: Start frontend
cd glove_web
npm run dev

# B4: Browser verification
# Open http://localhost:5173
# Expected: Dual 3D hands, WS connected, hands animate

# B5: Report
# - Does WS broadcast at 30fps?
# - Do flex values update in browser?
# - Do hands animate (curl changes)?
# - Any errors in browser console?
```

### Success Criteria
- Relay starts without errors
- Health endpoint returns `{"status":"ok"}`
- Frontend loads at `localhost:5173`
- Browser console shows WS connected
- 3D hands animate with mock data
- No JavaScript errors

### Report Format
```
## Mock E2E Demo Report (2026-07-04)

**Status**: [PASS/PARTIAL/FAIL]

**What Works**:
- [ ] Relay starts
- [ ] Health endpoint
- [ ] WS broadcast
- [ ] Frontend loads
- [ ] 3D hands render
- [ ] Hands animate

**What's Broken**:
- [Issue 1]
- [Issue 2]

**Gap List**:
1. [Missing piece for full E2E]
2. [Next step]

**Files Modified**:
- [List of files changed during debug]
```

---

## SOP Documentation Issues (Recorded for Future Fix)

These issues are **documented** but **not blocking** Approach A. Fix in LSM6DSV16X migration branch or separate PR.

### 1. I2C Frequency Inconsistency
**File**: `glove_firmware/include/data_structures.h`
**Issue**: Comment says "100kHz" but `I2CPins::FREQ=400000`
**Fix**: Update comment to 400kHz or change constant to 100000
**Priority**: Low (cosmetic)

### 2. GPIO8/9 Conflict Misinformation
**File**: `glove_firmware/test/test_i2c_scan/README.md`
**Issue**: Claims GPIO8/9 conflict with Octal PSRAM on N16R8 — **FALSE**
**Reality**: Octal PSRAM uses GPIO26-37, not GPIO8/9
**Fix**: Remove misinformation, align with `test_bno085/README.md`
**Priority**: Medium (causes confusion)

### 3. Redundant Wire.begin()
**File**: `glove_firmware/lib/Sensors/ADS1115Manager.h`
**Issue**: Calls `Wire.begin()` redundantly in `ADS1115Manager::begin()`
**Fix**: Remove if SensorManager already calls it
**Priority**: Low (harmless but sloppy)

### 4. BNO085 Init Skipped in V6
**File**: `glove_firmware/lib/Sensors/SensorManager.h:117`
**Issue**: BNO085 init commented out during V6 ADC debugging
**Fix**: Re-enable when LSM6DSV16X migration starts (different branch)
**Priority**: High (blocks IMU path) — **tracked in V6 migration plan**

### 5. LVGL BSP TODO Stale
**File**: `glove_firmware/p4_base_station/p4_firmware/main/display_task.cpp:89-93`
**Issue**: Comment shows intended BSP calls, not implemented
**Fix**: Phase A2c addresses this
**Priority**: High (blocking) — **will be fixed in this approach**

### 6. FramePairer Timeout Not Triggered
**File**: `glove_firmware/p4_base_station/p4_firmware/main/main.cpp:177`
**Issue**: `s_pairer.tick()` never called, timeout doesn't work
**Fix**: Phase A1 addresses this
**Priority**: Critical (data corruption) — **will be fixed in this approach**

---

## Success Metrics

### End of Phase A0
- [ ] Clean git status
- [ ] All uncommitted files committed and pushed
- [ ] Baseline commit: `git log -1 --oneline`

### End of Phase A1
- [ ] FramePairer timeout test passes
- [ ] C6 mock mode compiles
- [ ] C6 mock mode flashes without errors
- [ ] UART output shows 73-byte frames at 50Hz

### End of Phase A2
- [ ] A2a: TFLite stub returns `valid=true`
- [ ] A2b: USB CDC device appears on PC
- [ ] A2c: LVGL displays on 7" screen
- [ ] A2c: Audio plays from speaker (if PCM files exist)

### End of Phase A3
- [ ] C6→P4 UART data flows
- [ ] P4 pairs L/R frames
- [ ] P4 runs inference (stub)
- [ ] P4 displays gesture on screen
- [ ] P4 sends JSON via USB CDC
- [ ] PC sees JSON on `/dev/ttyACM*`

### End of Track B
- [ ] Relay starts with mock enabled
- [ ] Browser shows dual 3D hands
- [ ] Hands animate with mock data
- [ ] No console errors

---

## Timeline

| Phase | Duration | Parallelizable | Wall-Clock |
|-------|----------|----------------|------------|
| A0 | 10 min | No | 10 min |
| A1 | 1 hour | No | 1 hour |
| A2a | 30 min | Yes | — |
| A2b | 45 min | Yes | — |
| A2c | 1.5 hours | Yes | 1.5 hours |
| A3 | 1 hour | No | 1 hour |
| B | 30 min | Yes | 0 min (parallel) |
| **Total** | — | — | **4-5 hours** |

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| BSP component unavailable for P4 EV Board | Low | High | Use manual I2S/GPIO init if BSP fails |
| TinyUSB conflicts with ESP-IDF USB stack | Medium | Medium | Disable ESP-IDF USB, use TinyUSB only |
| LVGL memory overflow (PSRAM needed) | Low | High | Verify `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` |
| C6 UART TX pin conflict | Low | Low | Verify GPIO43 not used by other peripherals |
| Mock E2E demo fails | Medium | Low | Track B is background, doesn't block A |

---

## References

- **V5.2 Design Spec**: `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`
- **V5.2 Plan**: `docs/superpowers/plans/2026-06-10-v52-p4-base-station.md`
- **V6 ADC Spec**: `docs/V6/07_internal_adc_migration.md`
- **P4 Firmware**: `glove_firmware/p4_base_station/p4_firmware/`
- **C6 Firmware**: `glove_firmware/p4_base_station/c6_firmware/`
- **ESP-IDF Docs**: `https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/`
- **LVGL Docs**: `https://docs.lvgl.io/`
- **TinyUSB Docs**: `https://docs.tinyusb.org/`

---

## Next Steps

1. **Write implementation plan** — invoke `writing-plans` skill
2. **Execute A0** — commit baseline
3. **Launch A1 subagent** — FramePairer + C6 mock
4. **Launch Track B subagent** — mock E2E demo (background)
5. **Launch A2 subagents** — fill P4 stubs (parallel)
6. **Execute A3** — hardware verification
7. **Update docs + memory** — after each phase

---

*Design approved: 2026-07-04*
*Ready for implementation plan*
