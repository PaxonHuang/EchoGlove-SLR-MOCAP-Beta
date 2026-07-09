# PROGRESS.md — Cross-Session State Tracker

**Last updated**: 2026-07-03

---

## V6.0 LSM6DSV16X Migration

**Status**: Design documents written ✅ — pending user review
**Branch**: V6-LSM6DSV16X (to be created from V5-DualGloveFlex)
**Design Spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md`
**Date**: 2026-06-23

### Key Decision
Replace BNO085 ($15-25) with ST-LSM6DSV16X ($2-4) for cost savings + embedded SFLP fusion.

### Design Documents (6-file package)
| File | Status | Content |
|------|--------|---------|
| `docs/V6/01_architecture_diagrams.md` | ✅ Written | System architecture, data flow, I2C topology, FreeRTOS tasks |
| `docs/V6/02_BOM_table.md` | ✅ Written | Per-glove BOM, base station BOM, cost comparison |
| `docs/V6/03_wiring_diagram.md` | ✅ Written | LSM6DSV16X pinout, I2C bus, flex sensor circuit, UART wiring |
| `docs/V6/04_SOP-SPEC-PLAN_V6.md` | ✅ Written | Main 12-section design spec (supersedes V5.0+V5.2) |
| `docs/V6/05_claude_code_prompts.md` | ✅ Written | Implementation prompts for each migration phase |
| `docs/V6/06_decision_summary.md` | ✅ Written | Decision log, cost analysis, risk assessment |

### Migration Summary
- **IMU**: BNO085@0x4B → LSM6DSV16X@0x6A (SDO=GND)
- **SensorData interface**: UNCHANGED (11-dim: flex[5]+euler[3]+gyro[3])
- **ESP-NOW packet**: UNCHANGED (69 bytes)
- **Feature vector**: UNCHANGED (11 single, 28 dual)
- **New code**: ~400 lines (LSM6DSV16XManager ~200, MadgwickFilter ~150)
- **Changed code**: ~30 lines in SensorManager.cpp
- **Cost savings**: $26-42 per pair of gloves

### Next Steps
1. User reviews V6 design documents
2. Create V6-LSM6DSV16X branch
3. Phase 1: LSM6DSV16X driver implementation
4. Phase 2: SensorManager migration
5. Phase 3: End-to-end validation

---

## V5.2 P4 Smart Base Station

**Status**: All 8 tasks implemented ✅ — hardware testing in progress
**Latest commit**: `2dbe1e9` (ESP-IDF v5.4 build fixes)

### Environment Setup (2026-06-10)
- ESP-IDF v5.4 installed at `~/esp/esp-idf/`
- C6 firmware builds: `idf.py set-target esp32c6 && idf.py build` (OK)
- Source env: `source ~/esp/esp-idf/export.sh`

### Hardware Testing Progress

| Step | Description | Status | Commit/Notes |
|------|-------------|--------|-------------|
| 1a | S3 glove firmware build + flash | ✅ Done | `4af33a4`, port ttyACM1, ESP-NOW sending at 50Hz |
| 1b | C6 co-processor build + flash | ⏳ Blocked (needs USB-TTL adapter) | Build OK, needs 3.3V USB-TTL module |
| 1c | P4 firmware build + flash, verify UART | ✅ Done | `ttyACM0`, all subsystems OK, watchdog fix applied |
| 2 | S3 I2C scan (3 devices) | ✅ Done | 0x48+0x49+0x4B all detected, flat bus confirmed |
| 3 | BNO085 sensor data test | ✅ Done | Rotation vector, accelerometer, gyroscope streaming |
| 4 | ADS1115 I2C detection | ✅ Done | Both 0x48 and 0x49 detected, config registers OK |
**Branch**: V5-DualGloveFlex
**Design Spec**: `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`
**Implementation Plan**: `docs/superpowers/plans/2026-06-10-v52-p4-base-station.md`
**Timeline**: 3 weeks (deadline June 2026)

### Key Decisions
- ESP32-P4-Function-EV-Board v1.5.2 as smart base station (competition-provided)
- C6: ESP-NOW receiver + UART relay to P4
- P4: Tier2 inference (TFLite Micro, 28-dim, ~80KB INT8) + LVGL 7" display + TTS audio + USB HS
- UWB deferred to post-competition (V5.3)
- Glove firmware: zero changes
- 8 tasks total, TDD-first
- Model takes two 11-dim inputs (left, right) — not single 28-dim
- RelativeFeatures.compute() skipped (needs quaternion not in GlovePacket) — uses compute_relative_from_imu() instead

### Task Breakdown
| Task | Description | Commit | Status |
|------|-------------|--------|--------|
| 1 | Shared protocol library (UART framing) | `f4e4d34` | ✅ 12/12 tests |
| 2 | C6 ESP-NOW + UART relay firmware | `ef8d450` | ✅ |
| 3 | P4 UART receive + frame pairing + 28-dim assembly | `b3b011a` | ✅ |
| 4 | P4 Tier2 inference (TFLite Micro) | `ef1c84e` | ✅ (stub) |
| 5 | P4 LVGL display UI | `350ddb8` | ✅ (stub) |
| 6 | P4 TTS audio + USB CDC | `350ddb8` | ✅ (stub) |
| 7 | PC relay USB CDC input extension | `62b65d3` | ✅ 88/88 tests |
| 8 | Integration tests | `bef0c96` | ✅ |

### Test Summary (V5.2 additions)
| Component | New Tests | Total |
|-----------|-----------|-------|
| P4 native (UART frame + pairer + features) | 12 | 12 |
| Relay (USB CDC + integration) | 8 | 88 |

### Next Steps
1. ✅ **P4 flash + verify**: DONE — all subsystems init, watchdog fix applied
2. ✅ **S3 I2C scan**: DONE — 3/3 devices detected (0x48, 0x49, 0x4B), flat bus confirmed
3. ✅ **BNO085 sensor data test**: DONE — rotation vector, accelerometer, gyroscope streaming
4. ✅ **ADS1115 I2C detection**: DONE — both 0x48 and 0x49 detected, config registers OK
5. **ADS1115 raw ADC read**: verify analog input readings with known voltage
6. **Flex sensor integration**: connect flex sensors → verify ADC readings → calibration
7. **Full sensor integration**: BNO085 + ADS1115 + flex sensors together
8. **C6 flash**: use USB-TTL module (3.3V!) to flash C6 firmware
9. **C6→P4 UART link**: verify end-to-end data flow
10. **Model export**: run `python glove_firmware/scripts/export_model.py` when trained weights available
11. **LVGL BSP**: integrate with P4 EV Board 7" MIPI-DSI display
12. **ES8311 audio**: wire I2S via `esp_codec_dev` BSP component
13. **Competition demo**: full system integration + demo script

### P4 Verification Details (2026-06-12)
- **Port**: /dev/ttyACM0 (MAC 30:ED:A0:E2:24:B7, chip rev v1.3)
- **Boot**: ESP-IDF v5.4, 32MB PSRAM detected, all subsystems OK
- **Fixes applied**:
  - `uart_receiver.cpp`: cast `s_port` to `uart_port_t` (ESP-IDF v5.4 strict types)
  - `display_task.h`: add `#include "FramePairer.h"` for `FramePair` type
  - `main.cpp`: increase uart_task delay 1ms→10ms (watchdog fix when no C6 data)
- **Expected warnings**: model_data.h not found (stub), lvgl.h not found (log-only mode)

### S3 I2C Verification Details (2026-06-12, Updated 2026-06-22)
- **Port**: /dev/ttyACM1 (MAC 30:30:F9:21:D8:DC)
- **I2C bus**: Flat bus, GPIO8=SDA, GPIO9=SCL, 400kHz
- **Devices detected**: 3/3
  - 0x48: ADS1115 #1 (flex sensors 0-2)
  - 0x49: ADS1115 #2 (flex sensors 3-4)
  - 0x4B: BNO085 IMU
- **Key findings**:
  - BNO085 RST pin has strong internal pull-up — GPIO10 cannot pull LOW
  - Use power cycle (disconnect VCC 10s) to reset BNO085
  - PS0/PS1 must be GND for I2C mode (PS0=3.3V selects SPI mode)
  - Adafruit BNO08x v1.2.5 uses `begin_I2C()` not `begin()`
- **ADS1115 ADDR**: #1 ADDR→GND (0x48), #2 ADDR→VCC (0x49)
- **Sensor data**: BNO085 confirmed working — rotation vector, accelerometer, gyroscope streaming

### ADS1115 I2C Detection Details (2026-06-22)
- **Diagnostic firmware**: `glove_firmware/test/test_ads1115/test_ads1115_diagnostic.ino`
- **PlatformIO environment**: `[env:ads1115-diag]`
- **I2C scan results**:
  - 0x48: ADS1115 #1 (ADDR→GND) — config register 0x8583 (OK)
  - 0x49: ADS1115 #2 (ADDR→VDD) — config register 0x8583 (OK)
  - 0x4B: BNO085 IMU — detected
- **Total devices**: 3/3 — ALL DETECTED
- **Status**: PASS — I2C bus fully operational, all sensors ready
- **Next**: Phase 2 — raw ADC read test (verify analog input with known voltage)

---

## V5.0 DualGloveFlex Migration

**Status**: Phase 0-6 complete ✅, Phase 5/7 need hardware
**Branch**: V5-DualGloveFlex
**Design Spec**: `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md`
**Plan**: `docs/superpowers/plans/2026-06-01-v5-dual-glove-flex.md`

### Test Summary

| Component | Tests | Status |
|-----------|-------|--------|
| Firmware (native) | 64 | ✅ |
| Receiver (native) | 12 | ✅ |
| Relay (pytest) | 80 | ✅ |
| **Total** | **156** | **✅ All pass** |

### Phase Status

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ | Branch setup, V3 Hall/MUX cleanup |
| 1 | ✅ | Simulation + data structures |
| 2 | ✅ | Relay pipeline (Tier1CNN, CrossAttn, STGCN, Router) |
| 3 | ✅ | Receiver firmware (ESP-NOW, FramePairer, RelativeFeatures) |
| 4 | ✅ | Sensor layer (ADS1115, FlexManager, Kalman) |
| 5 | ⏳ | Data collection + model training (needs hardware) |
| 6 | ✅ | Frontend V5 dual-hand (types, stores, hooks, components) |
| 7 | ⏳ | E2E integration (needs hardware) |

### Cleanup Done (2026-06-03)

**Code cleanup:**
- Removed 14 V3 files (Hall/MUX/BLE/UDP/temp)
- Migrated `udp_server.py` + `main.py` to V5
- Created `scripts/calibrate.py` + updated `data_collector.py`
- Frontend: V5 types, dual-hand rendering, per-hand gesture display

**Docs cleanup (`23de267`):**
- Archived V3 hardware docs (ASSEMBLY, WIRING, PCB-Guide) to `docs/archive/v3/`
- Created V5 hardware guides (flat I²C, flex sensors, ESP-NOW, dual-hand)
- Consolidated `V5.0DualGloveFlex/`: archived GLM+Kimi drafts, kept MimoPro V5.1
- Updated `notebooks/README.md` with V5 constants (11/28-dim, flex sensors)
- Added V5 migration headers to all 3 Jupyter notebooks
- Updated `references/README.md` for V5 sensor list
- Copied V5 design spec + plan to `docs/superpowers/`
- Fixed `.gitignore` to track `docs/archive/`

---

## MCP Plugin Status (Updated 2026-05-15)

| Plugin          | Status  | Notes                                                                                                                  |
| --------------- | ------- | ---------------------------------------------------------------------------------------------------------------------- |
| Playwright      | WORKING |                                                                                                                        |
| Chrome DevTools | WORKING |                                                                                                                        |
| Context7        | FAILING | Proxy routing issue (`127.0.0.1:15721`), intermittent                                                                 |
| GitHub MCP      | FAILING | Token configured but needs session restart                                                                             |
| Espressif Docs  | FAILING | Proxy-related, intermittent                                                                                            |

---

## Session Continuation Protocol

When starting a new session:

1. Read this file first
2. Check the MCP status table — skip re-testing if verified recently
3. Continue from the last checkpoint below
4. Update this file when completing a task

---

## Windows → Ubuntu Migration (2026-05-15) — COMPLETE

All cross-platform compatibility issues resolved. Build passes on Ubuntu 24.04.

### Config Cleanup

| File | Action |
|------|--------|
| `.claude/settings.json` | Cleared broken Windows hook (`H:/HandSignRecognition/...`) |
| `.claude/settings.local.json` | Replaced with Ubuntu-native permissions (git, pio, npm, python) |
| `.gitignore` | Added `.claude/settings.local.json` for per-OS local config |

### Cross-Platform Line Endings

`.gitattributes` enforces LF for all source code, CRLF only for `.bat/.ps1/.cmd/.vbs/.reg`.

### Bugs Fixed During Migration (7 total)

| # | File | Problem | Fix |
|---|------|---------|-----|
| 1 | `.claude/settings.json` | Windows absolute path in hook `H:/HandSignRecognition/...` | Cleared |
| 2 | `.claude/settings.local.json` | 50+ PowerShell rules + `C:/Users/QuenchKidney/` paths | Ubuntu rules |
| 3 | `SensorManager.h:44` | TMAG5273 has no default constructor | Array init in member initializer list |
| 4 | `SensorManager.h:214` | `_imu.begin()` API changed (v1.2.5) | Changed to `begin_I2C()` |
| 5 | `SensorManager.h:240/274` | `_sensor_value.type` API changed | Changed to `.sensorId` |
| 6 | `FeatureNormalizer.h:34` | `FLT_MAX` not declared | Added `#include <cfloat>` |
| 7 | `TMG5273.h:45/74/83` | Class-internal `namespace` invalid C++ | `namespace` → `struct` + `;` |

### Build Status

`pio run` → **2 succeeded** (esp32-s3-devkitc-1-n16r8 + debug), 2026-05-15

---

## Phase 1 + Phase 2 Completion (2026-05-07)

### Phase 1: HAL & Driver Layer — COMPLETE

| Component                   | File                            | Status                                                                   |
| --------------------------- | ------------------------------- | ------------------------------------------------------------------------ |
| TCA9548A I2C mux driver     | `lib/Sensors/TCA9548A.h/.cpp` | Complete (disableAll→selectChannel two-step, 1ms bus delay)             |
| TMAG5273 Hall sensor driver | `lib/Sensors/TMG5273.h/.cpp`  | Complete (header-only, 32× avg, ±40mT, Set/Reset trigger)              |
| BNO085 IMU integration      | `lib/Sensors/SensorManager.h` | Complete (Game Rotation Vector + Calibrated Gyroscope @ 100Hz)           |
| SensorManager unified HAL   | `lib/Sensors/SensorManager.h` | Complete (I2C init, mux, Hall array, IMU, Kalman filtering, quat→Euler) |
| FlexManager placeholder     | `lib/Sensors/FlexManager.h`   | Complete (V3.0 returns zeros, V3.1 will use ADC)                         |
| FreeRTOS dual-core tasks    | `src/main.cpp`                | Complete (static_assert validation, correct parameter order)             |

### Phase 2: Signal Processing & Data Acquisition — COMPLETE

| Component                  | File                                | Status                                                          |
| -------------------------- | ----------------------------------- | --------------------------------------------------------------- |
| Kalman Filter 1D           | `lib/Filters/KalmanFilter1D.h`    | Complete (21 channels, auto-seed on first update)               |
| Sliding Window Ring Buffer | `lib/Filters/SlidingWindow.h`     | Complete (30×21 floats, PSRAM allocation, SPSC)                |
| Feature Normalizer         | `lib/Filters/FeatureNormalizer.h` | Complete (Min-Max [0,1], 2s calibration, per-channel stats)     |
| Pipeline integration       | `src/main.cpp`                    | Complete (readAll→toFeatureArray→normalize→push→queue→CSV) |
| Serial CSV output          | `src/main.cpp`                    | Complete (Edge Impulse data forwarder compatible)               |

### Signal Processing Pipeline Flow

```
SensorManager.readAll()     → SensorData (Kalman-filtered inside SensorManager)
SensorData.toFeatureArray() → float[21] features
FeatureNormalizer.updateStats() → during 2s calibration
FeatureNormalizer.normalize()  → features mapped to [0,1]
SlidingWindow.push()           → ring buffer (30 frames)
FreeRTOS queue send            → SensorData to g_data_queue
Serial CSV output              → Edge Impulse compatible format
```

### Unit Tests Created

| Test File                                        | Coverage                                                                    | Platform |
| ------------------------------------------------ | --------------------------------------------------------------------------- | -------- |
| `test/test_tca9548a/test_tca9548a.cpp`         | TCA9548A channel selection, disableAll, probe                               | ESP32    |
| `test/test_tmag5273/test_tmag5273.cpp`         | TMAG5273 begin, readXYZ, null mux handling                                  | ESP32    |
| `test/test_euler_conversion/test_euler_conversion.cpp` | quat→Euler (5), SlidingWindow (5), FeatureNormalizer (5)          | Native   |
| `test/test_inference_trigger/test_inference_trigger.cpp` | InferenceTrigger: confidence gating, debouncing, silent period (11) | Native   |
| `test/test_mock_model/test_mock_model.cpp`     | MockModel: init, preprocess, infer, postprocess, l2_requested (10)          | Native   |
| `test/test_inference_pipeline/test_inference_pipeline.cpp` | Pipeline: window→model→trigger integration (6)               | Native   |

**Native test count**: 42 pass / 2 errored (hardware-dependent) / 44 total

---

## Hardware Debug & Simulation Mode (2026-05-20) — IN PROGRESS

### Context

ESP32-S3 connected to Ubuntu via USB CDC (`/dev/ttyACM0`). Hardware partially wired:
- **PCA9548A mux** (兼容TCA9548A): Connected on GPIO8/9 with **2kΩ** pull-ups — **not responding on I2C** (err=5 NACK)
- **GY-BNO085 IMU**: Connected on mux CH5 (SD5→SDA, SC5→SCL) with **5.1kΩ** sub-bus pull-ups — **not responding** (depends on mux)
- **TMAG5273 Hall sensors**: Not connected (sensors not yet arrived, using simulation data)
- **BNO085 INT**: Connected to GPIO21

### Problems Fixed (5 total)

| # | Problem | Fix |
|---|---------|-----|
| 1 | Permission denied on `/dev/ttyACM0` | Installed udev rules (`/etc/udev/rules.d/99-platformio-udev.rules`), added to `dialout` group |
| 2 | ESP32-S3 USB CDC Serial not outputting after boot | Added `-DARDUINO_USB_CDC_ON_BOOT=1` to `platformio.ini` |
| 3 | Core dump checksum error blocking flash write | Erased flash with `pio run -t erase` before re-upload |
| 4 | I2C scanner hanging (ESP-IDF blocking on NACK) | Reduced scan to minimal address tests (0x70, 0x4B, 0x22) |
| 5 | Build error: braces around scalar initializer for float | Removed `PROGMEM`, used double brace syntax `{{...}, ...}` for GestureSignature array |

### Hardware Configuration Update (2026-05-21)

- **Pull-ups changed**: Main bus 5.1kΩ → **2kΩ** (closer to SOP 2.2kΩ spec, faster rise time at 400kHz)
- **Channel fix**: Firmware `MuxChannels::BNO085_IMU` changed from 7 → **5** to match hardware wiring (SD5/SC5)
- **Ch5 sub-bus pull-ups**: Installed **5.1kΩ** on SD5/SC5→3.3V for GY-BNO085
- **Hardware clarification**: MUX is **Adafruit TCA9548A 1-to-8 I2C Multiplexer Breakout** (not bare PCA9548A DIP chip). IMU is **7Semi GY-BNO085** module.
- **Ch0-4 sub-bus pull-ups**: Not installed (TMAG5273s not arrived). Main bus 2kΩ passes through TCA9548A internal switches — sufficient for testing.

### Simulation Mode Implementation

SensorManager now falls back to synthetic data generation when TCA9548A is not detected:

- **20 gesture classes** with distinct signatures (open hand, fist, thumb up, OK sign, peace, index point, etc.)
- Hall sensors: 0.1 (uncurled) / 0.9 (curled) + ±0.05 noise
- Euler angles: 0° to ±45° + ±2.0° noise
- Gyro: 0 deg/s (static gestures)
- Kalman filtering applied to synthetic data (validates signal processing pipeline)
- Auto-cycles gesture every 3 seconds (300 frames @ 100Hz)
- CSV output unchanged — compatible with Edge Impulse data forwarder
- 2-second calibration (FeatureNormalizer) then normalization to [0, 1]

### Files Modified

| File | Change |
|------|--------|
| `platformio.ini` | Added `-DARDUINO_USB_CDC_ON_BOOT=1` |
| `lib/Sensors/SensorManager.h` | Added simulation mode, 20 gesture signatures, `readSimulated()` method |
| `/etc/udev/rules.d/99-platformio-udev.rules` | Created — permanent serial port access |
| `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md` | Created — DM40B multimeter wiring verification guide |
| `docs/SESSION_SUMMARY_2026-05-20_HARDWARE_DEBUG_SIMULATION.md` | Created — full session summary |

### Hardware Debug History — RESOLVED (2026-05-26)

**Systematic isolation testing revealed the root cause:**

| Date | Finding |
|------|---------|
| 2026-05-22 | First I2C mux (bare PCA9548A DIP-16) defective — internal SDA↔SCL short (0.82kΩ) |
| 2026-05-24 | Discovered actual hardware is **Adafruit TCA9548A Breakout** (not bare PCA9548A) — corrected pinout assumptions |
| 2026-05-25 | Second Adafruit TCA9548A also failed — all NACKs. Confirmed **defective module** via direct BNO085 bypass test |
| 2026-05-25 | **BNO085 confirmed working** at address **0x4B** (not 0x4A) via direct I2C bypass. Firmware updated. |
| 2026-05-26 | Third TCA9548A (PW548A TI chip) installed. Direct wire test: **TCA9548A confirmed ACK at 0x70** when bypassing breadboard. |

**Root cause**: **Breadboard contact failure**. All 3 mux modules were likely functional — the breadboard's spring contacts degraded after multiple insertions/removals, causing intermittent I2C connections. Physical bus test (GPIO toggle + pull-up recovery) passed, but I2C protocol failed because the breadboard introduced resistance/intermittency at the signal level.

**Key diagnostic evidence:**
- Physical bus test: SDA/SCL toggle OK, recovery 0µs → pull-ups and GPIO healthy
- Hardware I2C: all NACK/TIMEOUT → protocol-level failure
- Bit-bang I2C: all NACK → same, not ESP32 peripheral issue
- **Bypass breadboard with Dupont wires: TCA9548A found at 0x70 ✓** → breadboard confirmed as root cause

**Current hardware status:**
- **Adafruit TCA9548A (PW548A chip)**: **Working** — confirmed via direct Dupont wire connection
- **BNO085**: Working at address **0x4B**. Firmware updated.
- **TMAG5273 Hall sensors**: Not yet installed (awaiting new breadboard + sub-bus pull-ups)
- **I2C bus**: Confirmed functional when bypassing breadboard

**Action required**: Buy new breadboard, new Dupont wires, and reassemble. User purchasing tonight (2026-05-26).

---

## Active Work

**Current**: Phase 5 Python Relay Server COMPLETE (133/133 tests). Hardware testing paused — user purchasing new components (breadboard, Dupont wires, TCA9548A, TMAG5273 replacements). Tomorrow: Phase 1–4 hardware re-verify.

### Phase 5 Completion Summary (2026-05-28)

| Component | Tests | Status |
|-----------|-------|--------|
| Protobuf parser | 19/19 | Done |
| UDP server | 23/23 | Done |
| WebSocket manager | 15/15 | Done |
| ST-GCN model | 27/27 | Done |
| NLP grammar corrector | 15/15 | Done |
| TTS engine | 13/13 | Done |
| ConfidenceRouter | 11/11 | Done |
| Integration tests | 10/10 | Done |
| **Total** | **133/133** | **ALL GREEN** |

New files created:
- `src/confidence_router.py` — L1→L2 confidence-driven routing (extracted from UDPServer)
- `tests/test_tts_engine.py` — TTS engine tests (mocked edge_tts via sys.modules injection)
- `tests/test_confidence_router.py` — Router logic tests
- `tests/test_integration.py` — FastAPI app lifecycle tests

Modified files:
- `src/main.py` — Added `/api/status`, `/api/tts/audio` endpoints; wired NLP+TTS+ConfidenceRouter into lifespan
- `src/udp_server.py` — Accepts optional `router: ConfidenceRouter` parameter

### Conda Environments Ready

| Environment | Python | PyTorch | Use Case | Relay Tests |
|-------------|--------|---------|----------|-------------|
| `pytorch21_env` | 3.9.25 | 2.1.0+cpu | ST-GCN training, relay dev | **133/133 ✓** |
| `tf216` | 3.11.9 | 2.1.0+cpu | TFLite training, model export | **133/133 ✓** |

### Phase Status Summary

| Phase | Name | Status |
|-------|------|--------|
| P0 | Project init | Done |
| P1 | HAL & drivers | Done (code) — **needs hardware re-verify with new breadboard** |
| P2 | Signal processing | Done (code) — **needs hardware re-verify** |
| P3 | L1 Edge Inference — Pipeline + TDD | Done (42/44 native tests) |
| P3.5 | Model Benchmark | Pending |
| P4 | Communication (BLE/UDP/Protobuf) | Done (133/133 relay tests) |
| P5 | Python Relay + L2 ST-GCN + NLP + TTS | **Done** (133/133 tests) |
| P6 | Web rendering / Unity Pro | Scaffold exists |
| P7 | Integration testing | Pending |

### Next Steps (ordered)

1. **Hardware re-verify** (tomorrow): I2C scan on new breadboard → sensor validation → CSV output
2. **Edge Impulse data collection** (Path A MVP): train L1 1D-CNN model
3. **Model export**: TFLite → integrate into firmware
4. **Phase 6**: React + R3F frontend (WebSocket consumer + 3D hand skeleton)

---

## Phase 3: L1 Edge Inference — TDD Completion (2026-05-22)

### TDD Red-Green-Refactor Summary

**InferenceTrigger** (Deliverable E — SOP §6.6):
- RED: 9 fail, 2 pass (stub returns defaults)
- GREEN: 11/11 pass — confidence threshold 0.85, 5-frame debounce, 100ms silent period
- Implementation: `lib/Inference/InferenceTrigger.h` (97 lines, header-only)

**MockModel** (pipeline testing enabler):
- RED: 9 fail, 1 pass (stub returns defaults)
- GREEN: 10/10 pass — configurable output, softmax/argmax postprocess, l2_requested band
- Implementation: `lib/Models/MockModel.h` (header-only, no Arduino/TFLite dependency)

**InferencePipeline** (pipeline glue):
- RED: 3 fail, 3 pass (stub returns false)
- GREEN: 6/6 pass — SlidingWindow → ModelRegistry → InferenceTrigger flow
- Implementation: `lib/Inference/InferencePipeline.h` (header-only)

**Task_Inference wiring**:
- ModelRegistry + InferenceTrigger globals instantiated in main.cpp
- Task_Inference now calls `runInferencePipeline()` on active model
- Confirmed gestures pushed to `g_inference_queue` as `InferenceResult`
- ESP32 build: both envs pass (regular + debug)

### Infrastructure Fixes
- `lib/data_structures.h` → redirect to `include/data_structures.h` (single source of truth)
- `include/data_structures.h` → `#ifdef UNIT_TEST` stubs (Serial, ps_malloc, PROGMEM)
- `platformio.ini` → added `[env:native]` for fast TDD cycles (~1s vs ~100s ESP32)
- Test directories renamed `test_*` prefix (PlatformIO discovery requirement)
- Arduino `setup()/loop()` stubs in all test files
- TFLiteModel.h → fixed `MicroInterpreter` constructor for new API (ErrorReporter param)

### Phase 3 Deliverable Status

| Deliverable | Code | Tested | Notes |
|-------------|------|--------|-------|
| A. Edge Impulse MVP | Deferred | — | Needs data collection |
| B. 1D-CNN+Attention training | Written | No | Needs training data |
| C. MS-TCN training | Written | No | Needs training data |
| D. BaseModel + Registry + Hot-Switch | Done | Framework | `BaseModel.h`, `ModelRegistry.h` |
| E. Inference Trigger | Done | **11/11** | TDD complete |
| F. TFLite Micro integration | Done | Build passes | Needs `model_data.h` (trained model) |

### Blocking Dependency

**Training data** blocks: Deliverables A/B/C/F full validation, Phase 3.5 benchmarks.
Simulation mode provides synthetic 20-gesture data for pipeline testing.

---

## Phase 5: Python Relay — TDD Infrastructure (2026-05-22)

### Protobuf Schema Sync

Firmware `.proto` established as single source of truth. Relay's `glove_data.proto` overwritten with firmware's canonical version (package `data_glove`, `hall_features` float, `l1_gesture_id`, `l1_confidence`, `l2_requested`, `status` string). Python `glove_data_pb2.py` regenerated via `grpcio-tools`.

### TDD Red-Green-Refactor Summary

**protobuf_parser** (19 tests):
- GREEN: 19/19 — parse valid protobuf, invalid data handling, round-trip, `build_glove_data_dict` helper
- Proto3 empty-bytes behavior: returns defaults (not error)

**UDPServer** (23 tests):
- GREEN: 23/23 — construction, datagram handling, L1→L2 routing, debounce, silence period, buffer accumulation
- Bug fix: `_last_gesture_time` was set on every buffer append (prematurely blocking frames within silence_ms). Moved to only update after L2 actually fires. This was a real bug found by TDD.

**ST-GCN Model** (27 tests):
- GREEN: 27/27 — adjacency matrix, GraphConv, TemporalConv, STConvBlock, AttentionPooling, full model end-to-end
- Verified: output shapes, gradient flow, predict API, config serialization

**WebSocket ConnectionManager** (15 tests):
- GREEN: 15/15 — connect/disconnect lifecycle, broadcast to all/JSON/unicode, dead client removal, `close_all` graceful shutdown

**Full relay test suite**: 84/84 pass in 2.43s

### Test Files

| Test File | Tests | Coverage |
|-----------|-------|----------|
| `tests/test_protobuf_parser.py` | 19 | Protobuf decode, invalid data, round-trip, dict builder |
| `tests/test_udp_server.py` | 23 | Server init, datagram handling, L1→L2 routing, debounce, silence |
| `tests/test_stgcn_model.py` | 27 | Adjacency, GraphConv, TemporalConv, STConvBlock, AttnPool, STGCNModel |
| `tests/test_ws_server.py` | 15 | Connection lifecycle, broadcast, cleanup |

### Bug Found by TDD

**Silence period gating bug** (`udp_server.py:158`): `_last_gesture_time = now` was inside the outer `if` block (executed on every low-confidence frame that passes debounce). This caused the silence period to block ALL subsequent frames for 800ms after any buffer append, not just after L2 fires. Fix: moved `_last_gesture_time = now` inside the `if len(buffer) >= window_size` block where L2 actually triggers.

### Dependencies Installed

`fastapi`, `websockets`, `pyyaml`, `numpy`, `protobuf`, `grpcio-tools`, `pytest`, `pytest-asyncio`, `torch` (CPU)

---

## 2026-06-18 — BNO085 Diagnostic Session (N16R8 + GY-BNO085)

**Status:** Two SOP/code bugs identified & fixed; diagnostic firmware deployed; awaiting hardware flash + serial output.

### Bugs Found & Fixed

**Bug 1: `platformio.ini` wrong board config (CRITICAL)**
- Symptom: env named `-n16r8` but configured like N8 (no PSRAM, 8MB flash, qio)
- File: `glove_firmware/platformio.ini` lines 14-19
- Fix: `default_16MB.csv` + `psram=enable` (kept `qio` flash_mode since N16R8 uses Quad flash + Quad PSRAM, not Octal)
- Impact: PSRAM was not initialized; heap/buffer failures could corrupt Wire state
- Build verified: `pio run` → SUCCESS (RAM 6.8%, Flash 4.7%)

**Bug 2: `main.cpp` pin violation vs SOP**
- Symptom: SDA=11, SCL=12, INT=9, RST=10 (all wrong, deviated from SOP)
- SOP requirement: SDA=8, SCL=9, INT=1, RST=-1 (hardwired 3V3)
- File: `glove_firmware/src/main.cpp`
- Fix: replaced with SOP-compliant 5-phase diagnostic firmware (Phase 0/1/1.5/2/3)
- Backup: `glove_firmware/src/main.cpp.diag_v1`

### Diagnostic Firmware Capabilities
- Phase 0: print SOP pin map + manual checklist
- Phase 1: I²C scan at 100/50/10 kHz (multi-frequency fallback)
- Phase 1.5: raw register read from 0x4B
- Phase 2: `begin_I2C()` + enable ROTATION_VECTOR (100Hz) + ACCELEROMETER (50Hz)
- Phase 3: 10s streaming with `wasReset()` watchdog

### Next Steps
1. Flash: `cd glove_firmware && pio run -t upload --upload-port COMx`
2. Monitor: `pio device monitor -b 115200`
3. Copy full output for Phase-by-Phase analysis
4. If Phase 1 empty → re-verify wiring (4.7kΩ pull-ups, ADO=3V3, PS0=3V3, PS1=GND, RST=3V3)
5. If Phase 2 fails → 5s power cycle then retry (BNO085 boot settling ~500ms)

### Reference
- Plan: `docs/superpowers/plans/2026-06-18-bno085-diagnostic-firmware.md`
- SOP docs (verified consistent): `docs/HARDWARE_ASSEMBLY_GUIDE.md`, `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B_CN.md`

---

## 2026-06-21 — BNO085 Diagnostic Session (Windows 11 + N16R8 + GY-BNO085)

**Status:** BNO085 fully operational — sensor data streaming confirmed ✅
**Environment:** Windows 11, ESP32-S3-DevKitC-1 N16R8, PlatformIO
**Next:** Ubuntu 24.04 compatibility verification

### Wiring (Final — Working)

| BNO085 Pin | ESP32-S3 Pin | Notes |
|------------|--------------|-------|
| VCC | 3.3V | **NOT 5V!** |
| GND | GND | |
| SDA | GPIO8 | 4.7kΩ pull-up to 3.3V |
| SCL | GPIO9 | 4.7kΩ pull-up to 3.3V |
| CS | 3.3V | HIGH for I2C mode |
| PS0 | GND | **GND=I2C, 3.3V=SPI** |
| PS1 | GND | Must be GND |
| ADO | 3.3V | HIGH=0x4B, LOW=0x4A |
| RST | GPIO10 | Optional: 3.3V if not needed |
| INT | Floating | Not used |

### Critical Lessons Learned

1. **PS0/PS1 are latched at power-up**: PS0=3.3V selects SPI mode and can damage the module!
2. **GPIO8/9 works on N16R8**: Unlike N8 variant where GPIO8/9 conflict with internal flash
3. **Adafruit BNO08x v1.2.5 API**: Uses `begin_I2C()` not `begin()`, `sh2_SensorValue_t` uses `.sensorId` not `.type`
4. **BNO085 needs power cycle**: Fails if in "hot" state from previous attempts
5. **RST pin has strong internal pull-up**: GPIO10 cannot pull it LOW — use power cycle instead
6. **CS must be HIGH**: 3.3V for I2C mode
7. **ADO selects address**: HIGH=0x4B, LOW=0x4A

### Test Results

**Diagnostic Test:**
- I2C scan: BNO085 found at 0x4B ✅
- RST pin: HARD HIGH (expected — strong internal pull-up)
- Wiring: All correct

**Sensor Data Test:**
- BNO085 initialization: SUCCESS ✅
- Rotation vector: Streaming (200Hz) ✅
- Accelerometer: Streaming (100Hz) ✅
- Gyroscope: Streaming (100Hz) ✅
- Data quality: Normal (Z≈9.8 m/s² when stationary)

### Files Created

| File | Purpose |
|------|---------|
| `glove_firmware/test/test_bno085/test_bno085_diagnostic.ino` | I2C scan + RST pin check |
| `glove_firmware/test/test_bno085/test_bno085_sensor_data.ino` | Full sensor data streaming |
| `glove_firmware/test/test_bno085/README.md` | Test documentation |

### Documentation Updated

- `PROGRESS.md` — This section
- `PROGRESS_CN.md` — Chinese version
- `CLAUDE.md` — Hardware context, Adafruit library notes
- `memory/s3-n8-gpio8-9-i2c-bug.md` — Updated for N16R8
- `memory/burned-modules-lesson.md` — Added PS0/SPI mode damage
- `memory/bno085-adafruit-library.md` — New: Adafruit library requirements
- `docs/BNO085_DIAGNOSTIC_GUIDE.md` — Updated wiring and troubleshooting
- `docs/HARDWARE_ASSEMBLY_GUIDE.md` — Updated BNO085 section
- `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B_CN.md` — Updated Chinese guide

### Next Steps

1. **Ubuntu 24.04 verification**: Run same tests on Ubuntu to confirm cross-platform compatibility
2. **ADS1115 test**: Connect flex sensors → verify ADC readings
3. **Full sensor integration**: BNO085 + ADS1115 + flex sensors together
4. **Data collection**: Start collecting gesture data for model training

### Windows → Ubuntu Compatibility Notes

- GPIO8/9 pin assignments: Same on both platforms
- Adafruit BNO08x library: Works on both platforms
- PlatformIO build: Works on both platforms
- Serial monitor: `pio device monitor` works on both (COM6 on Windows, /dev/ttyACMx on Ubuntu)
---

## 2026-06-28: V5 硬件诊断 + 模拟数据开发线（ADS1115 硬件阻塞，转 mock 推进）

### 硬件诊断结论（关键）
- **3.3V 电源轨正常**（3.29V）
- **I²C 总线电压异常**：完整电路时 SDA=1.20V / SCL=1.19V（远低于 V_IH_min≈2.31V）
- **隔离测试定位元凶**：拔下两块 ADS1115 后总线恢复 3.20V/3.19V → **ADS1115 把总线拉低**
- **关键异常**：ADS1115 的 SDA↔SCL 之间只有 **300Ω**（正常应 >MΩ），疑似芯片/模块损坏（ESD/过压/出厂不良）
- **BNO085 接线正确**：PS0=GND, PS1=GND, ADO=3.3V → 0x4B
- **ESP32-S3 GPIO8/9 无问题**：单拔 ESP32 后总线 3.23V，非 strapping pin，N16R8 上无 PSRAM 冲突

### 待办（硬件，等用户处理）
- [ ] 单块 ADS1115 隔离测试，定位损坏的具体是哪一块
- [ ] 更换损坏的 ADS1115 模块
- [ ] 总线电压恢复 ~3.3V 后，烧录 `pio run -e v5-diag -t upload` 全面诊断

### SOP 文档错误（待修正，已记录在 task #5）
1. `docs/V5.0DualGloveFlex/.../03_wiring_diagram.md` 第581-588行 BNO085 接线图错误：
   - PS0 标注为 VDD（错误，应为 GND；PS0=VDD 选 SPI 模式甚至损坏模块）
   - RST 接到 GND（错误，应为 3.3V 或悬空，GND 会永久复位）
   - ADO/CS 未显式标注
2. `glove_firmware/test/test_i2c_scan/README.md` 声称 GPIO8/9 与 Octal PSRAM 冲突 → 与 CLAUDE.md/test_bno085/README.md **矛盾**，该说法对 N16R8 DevKitC-1 **错误**（Octal PSRAM 用 GPIO26-37，非 8/9）
3. `data_structures.h` 注释说 100kHz 但 `I2CPins::FREQ=400000`，SOP 文档说 100kHz → 频率不一致需统一
4. `SensorManager.h:117` 故意跳过 BNO085 init（调试 ADS1115 期间）→ 硬件修复后需重新启用
5. `ADS1115Manager::begin()` 有冗余 `Wire.begin()` 调用 → 修复后清理

### 模拟数据开发线（当前主线，绕过硬件阻塞）
**策略**：用软件 mock 数据推进 Relay+Web 链路，硬件修好后 `simulation=false` 切回真实数据。

**已完成**：
- 新建 `glove_relay/src/mock_data.py` — `MockDataSource` 类，30fps 生成 V5 双手格式消息（5种手势循环+插值+噪声）
- `glove_relay/tests/test_mock_data.py` — 6个测试（schema/range/cycle/pacing/stop/json）
- `glove_relay/src/utils/config.py` — 新增 `_MockConfig` 配置段
- `glove_relay/configs/relay_config.yaml` — 新增 `mock:` 段（enabled: true）
- `glove_relay/src/main.py` — lifespan 集成 mock，mock 模式跳过 model registry（避免缺 checkpoint 报错）
- 测试通过：`python -m pytest tests/test_mock_data.py tests/test_ws_format_v5.py tests/test_ws_server.py -q` → **22 passed**
- Relay 服务器实测启动成功：health=ok，mock 模式生效，日志显示 30fps

**进行中 / 待验证**：
- [ ] WebSocket 数据流验证（写了 `/tmp/test_ws_client.py`，但结果未读）
- [ ] glove_web 前端联调（`npm run dev`，浏览器看 3D 手渲染）
- [ ] 更新 glove_relay README、PROGRESS_CN.md

**执行计划文件**：`.claude/plans/relay_web_mock_data.md`（7步，当前在 Step 5/6）

### 新会话恢复指引
1. **硬件线**：等用户反馈单块 ADS1115 隔离测试结果 → 换模块 → 烧录 v5-diag
2. **模拟线（优先）**：续跑 `.claude/plans/relay_web_mock_data.md` Step 5-7
   - 启动 relay：`cd glove_relay && python -m src.main`（要先 `kill $(lsof -ti:8765)` 清端口）
   - 验证 WS：`python /tmp/test_ws_client.py`
   - 联调前端：`cd glove_web && npm run dev`
3. 已知小问题：`serial_asyncio` 模块缺失（USB CDC 路径报错，mock 模式下不影响；真实 P4 联调时需 `pip install pyserial`）

---

## V6 内部 ADC 迁移（2026-07-02~03，分支 feature/v6-dual-s3p4-flex-lsm6dsv16x）

**决策**：V6 去 ADS1115×2，改用 ESP32-S3 内部 ADC1 (GPIO1-5) 读 5 弯曲传感器。配合已有的 BNO085→LSM6DSV16X IMU 迁移。

**4 个关键决策（用户 AskUserQuestion 确认）**：
1. ADC API = `analogReadMilliVolts()` + N=16 软件过采样（~12-13 effective bits, <1ms/frame）
2. 标定存储 = NVS `Preferences` key-value（修 V5 RAM-only bug）
3. 抽象层 = 新增 `IFlexSensor` 接口（Strategy/Adapter，可切回 ADS1115）
4. 分压+衰减 = 保 47kΩ + `ADC_ATTEN_DB_12`（硬件零改动）

### 实现进度（2026-07-03）

**分支**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
**状态**: Tasks 1-5 DONE & committed (NOT pushed)，Tasks 6-7 pending
**实现计划**: `docs/superpowers/plans/2026-07-03-v6-internal-adc-migration.md` (7 tasks, TDD-first)

#### 已完成 (6 commits)

| Commit | Task | 内容 |
|---|---|---|
| `94bcfdb` | T1 | `IFlexSensor.h` 接口 + 10 mock 测试（begin/readRaw/readMilliVolts/persistCalibration/hasCalibration/getCalibrationBounds） |
| `e1c3f5e` | T3 | `InternalADCManager.h` (ADC1 GPIO1-5, N=16 oversample, Kalman, NVS stub) + 16 测试 |
| `abbff5f` | T2 | `FlexManager.h` 重构为依赖 `IFlexSensor*` (不再依赖 ADS1115Manager) + 8 测试 |
| `7b28858` | T4 | `test_adc_calibration` (4 测试); 修复 `persistCalibration` 在 UNIT_TEST 模式下设置 _has_calib |
| `7ed6624` | T5 | `SensorManager.h` 持有 `IFlexSensor*`; `main.cpp` 恢复 V5 FreeRTOS 任务架构 (`SIMULATION=false`); `platformio.ini` +Preferences lib_dep +`lib_ldf_mode=deep+`; `ADC_ATTEN_DB_12`→`ADC_11db` 兼容宏 |
| `1c6f4dc` | — | 清理冗余文件（log/、graphify-out/、.claude/plans/ 加入 .gitignore）；优化 `__pycache__` 规则；添加 V6 实现计划 |

#### 构建/测试状态

- **硬件构建**: `pio run -e esp32-s3-devkitc-1-n16r8` = ✅ SUCCESS (RAM 13.4%, Flash 22.4%)
- **Native 测试**: 38/38 pass (4 个 V6 测试套件: test_iflex_sensor, test_internal_adc, test_flex_manager, test_adc_calibration) ✅
- **Git 状态**: 6 次 commit，分支**未 push** 到远程

#### 待完成任务

- **Task 6 (可选)**: `ADS1115FlexAdapter.h` V5 兼容适配器 — 将现有 `ADS1115Manager` 包装为 `IFlexSensor` 实现，让 V5 硬件仍能通过 V6 FlexManager 工作。用户确认**暂不需要**。
- **Task 7**: On-device 验证 V1-V7 (07 §8) — 需要物理 ESP32-S3 + 弯曲传感器。V1=GPIO boot-strap, V2=ADC1↔ESP-NOW 共存, V3=吞吐量<1ms, V4=ENOB≥12, V5=NVS 持久化, V6=L1 精度, V7=分压线性度。可延后到硬件接入时。

#### 关键兼容性笔记（后续会话参考）

- **`ADC_ATTEN_DB_12` 未声明**: Arduino-ESP32 core 2.x (espressif32@^6.5.0) 使用 legacy `ADC_11db` 枚举名。已通过 `#ifndef ADC_ATTEN_DB_12 #define ADC_ATTEN_DB_12 ADC_11db` 兼容。07 spec §3.3 提到 DB_11 是 deprecated alias — 两者等效。
- **`Preferences.h` 未找到**: 必须在 `platformio.ini` 中添加 `Preferences` 到 `lib_deps` 并且设置 `lib_ldf_mode = deep+`。默认 chain mode 只扫描源文件，不扫描头文件 — `InternalADCManager.h` 在头文件中 include `<Preferences.h>` 会被遗漏。
- **`persistCalibration` 在 UNIT_TEST**: 必须在 `#else` 分支设置 `_has_calib=true`，不能只在 `#ifndef UNIT_TEST` 分支。Native 测试没有真实 NVS 但仍需验证标志切换。

#### 本次会话设计决策（AskUserQuestion 确认）

1. **IMU 路径**: 仅 ADC 迁移 — BNO085 仍 SKIP，IMU 返回零值。LSM6DSV16X 迁移是独立 V6 任务（见 `project_v6_migration_plan.md`）。`SensorManager.readHardware()` 留了 LSM6DSV16XManager 的 TODO 占位。
2. **main.cpp**: 恢复 V5 应用 + V6 改造 — 从 commit `4af33a4` 恢复 FreeRTOS 双核任务架构（Task_SensorRead@Core1 100Hz + Task_Comms@Core0 50Hz + ESP-NOW broadcast），改为 `SIMULATION=false` 走 V6 内部 ADC 硬件路径。原 bit-bang I2C 诊断程序已覆盖。

### 文档完成（2026-07-02）

- ✅ 新建+切换 git 分支 `feature/v6-dual-s3p4-flex-lsm6dsv16x`
- ✅ 新建 `docs/V6/07_internal_adc_migration.md`：完整深度 spec（ENOB 定量论证 / IFlexSensor / InternalADCManager.h 实现 / NVS 标定 / ADC1/ADC2 共存 / 验证计划 / stale-fix 清单 / 工程权衡矩阵 / 决策日志）
- ✅ 升级 `docs/V6/` 01-06 + README：消除所有 "ADS1115 unchanged" stale 点
- ✅ BOM 更新：per-glove ¥55→¥47，per-pair system ¥256-286→¥196-226（省 ¥60-80/pair）
- ✅ `docs/archive/README.md` 重写为版本索引

### 下一步

1. `git push` 完成（用户手动执行后 `/new` 重开会话）
2. Task 7 (硬件验证) — 需要物理设备
3. 完成后更新 `PROGRESS.md` + `PROGRESS_CN.md`（本次已完成）
4. 继续处理未暂存的 glove_relay 修改（mock_data 等）

---

## P4 Base Station Hardware Verification (A3) — 2026-07-08

### Hardware Setup
- **P4**: ESP32-P4-Function-EV-Board v1.5.2, `/dev/ttyACM0` (USB Serial/JTAG)
- **C6**: On-board ESP32-C6-MINI-1, pre-flashed ESP-Hosted slave firmware v0.0.6
- **CH340 TTL adapter**: Connected to PROG_C6 header (`/dev/ttyUSB0`), detected as ESP32-C6FH4

### Architecture Discovery (Critical)
1. **ESP-Hosted does NOT support ESP-NOW**: The on-board C6 is a Wi-Fi/BT co-processor over SDIO, not a general-purpose MCU. The original `c6_firmware` mock-ESP-NOW bridge is **incompatible** with the P4 EV Board's C6.
2. **C6 flashing**: PROG_C6 header supports UART flashing via ESP-Prog or CH340 TTL adapter. Before flashing, P4 must be put in bootloader mode (hold BOOT + press RST, or `esptool.py -p <host_port> --before default_reset --after no_reset run`).
3. **OTA**: Only updates ESP-Hosted slave firmware (validated images), NOT arbitrary custom firmware.
4. **ESP-Serial-Flasher**: Alternative — P4 can flash C6 over direct UART GPIO connection (dedicated UART, not the ESP-Hosted SDIO bus).

### A3 Execution Path Chosen
- **Option: P4-only standalone verification** (user decision)
- C6 left as factory ESP-Hosted Wi-Fi/BT co-processor (not flashed)
- Added `CONFIG_P4_INTERNAL_MOCK=y` Kconfig flag + `mock_data_source` module to P4 firmware
- Generates synthetic L/R GlovePackets at 50Hz/hand, feeds `FramePairer` directly

### P4 Hardware Verification Results
| Subsystem | Status | Evidence |
|-----------|--------|----------|
| Boot | ✅ PASS | ESP-IDF v5.4, 32MB PSRAM detected, app loads from flash |
| LVGL Display | ✅ PASS | BSP display_start, 1024×600 MIPI-DSI, LVGL widgets created |
| FramePairer | ✅ PASS | L/R pairs formed correctly (Pair tick=0..N logs) |
| TFLite Stub | ✅ PASS | Stub cycling: `Tier2 #N: gesture=4 conf=0.800 time=2 us` |
| Display Update | ✅ PASS | `Gesture: 你好 (80%)`, `Status: C6=0 P4=1 Tier=2` |
| ES8311 Audio | ✅ INIT | Codec initialized @ 16kHz/16bit/mono; PCM files absent (no SD card) |
| USB CDC (TinyUSB) | ✅ INIT | TinyUSB HS CDC enumerated; USB cable to PC needed for JSON output |
| LCD DSI Underrun | ⚠️ KNOWN | `lcd.dsi.dpi: can't fetch data from external memory fast enough` — P4 EV Board PSRAM bandwidth issue, cosmetic only |
| SD Card | ❌ N/A | No SD card inserted; `sdmmc_init_ocr: send_op_cond returned 0x107` |

### Commits (this session)
| SHA | Description |
|-----|-------------|
| `4f541bb` | feat(p4): internal mock data source for standalone verification (A3) |
| `3cf2f3a` | fix(web+relay): WebSocket 403 → use /ws endpoint; add pyserial-asyncio dep |

### Track B (Mock Relay→Browser E2E) — COMPLETED (prior session)
- WebSocket 403 bug fixed (`/ws` endpoint)
- `pyserial-asyncio` dep added
- Verified: relay mock @30fps → WS → React dashboard, 0 JS errors, FPS=32

### Remaining Work
- **USB CDC JSON output**: Need USB cable from P4 USB HS port → PC to read inference JSON
- **SD card + TTS**: Insert microSD with `/tts/*.pcm` files for audio verification
- **C6 Wi-Fi/BT**: Integrate ESP-Hosted into P4 firmware for Wi-Fi connectivity (future phase)
- **LCD DSI underrun**: Investigate PSRAM bandwidth tuning (low priority)

---

## V5.3 Wired Dev Path — S3→P4 Direct UART (2026-07-09)

### Architecture Pivot Decision
The on-board C6 is an ESP-Hosted Wi-Fi/BT co-processor (SDIO bus, factory pre-flashed). ESP-Hosted does **NOT support ESP-NOW**. Original `c6_firmware` mock-ESP-NOW bridge incompatible.

**Decision**: S3 → P4 direct UART (bypass C6). C6 deferred to Wi-Fi integration phase.
- Branch: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
- Tag: `v5.3-wired-dev` (baseline)
- GPIO audit: S3 GPIO6 (TX) FREE → P4 GPIO38 (RX). Dual-hand via dual UART (physical isolation, no bus contention).
- Design: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`
- Plan: `docs/superpowers/plans/2026-07-09-s3-p4-wired-uart.md`

### Implementation Status
- [ ] Phase 1: S3 UARTTransmitter.h + native tests (TDD)
- [ ] Phase 2: S3 main.cpp parallel UART TX (WIRED_UART=1)
- [ ] Phase 3: P4 sdkconfig CONFIG_P4_INTERNAL_MOCK=n
- [ ] Phase 4: Hardware wiring + end-to-end verification
- [ ] Phase 5: Dual-hand (second P4 UART)

### Docs Updated (this session)
- ✅ CLAUDE.md: added V5.3 section at top
- ✅ docs/V6/03_wiring_diagram.md: §5 S3↔P4 direct UART (active), §6 C6↔P4 (deferred), renumbered sections
- ✅ docs/V6/01_architecture_diagrams.md: added architecture update notice
- ✅ docs/V6/04_SOP-SPEC-PLAN_V6.md: added communication update notice + corrected branch name
- ✅ Memory: `p4-ev-board-c6-esp-hosted.md`, updated `project-p4-base-station-verification-progress.md`
