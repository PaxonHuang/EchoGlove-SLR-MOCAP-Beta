# PROGRESS.md — Cross-Session State Tracker

**Last updated**: 2026-05-21

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

| Test File                           | Coverage                                                                    |
| ----------------------------------- | --------------------------------------------------------------------------- |
| `tests/test_tca9548a.cpp`         | TCA9548A channel selection, disableAll, probe                               |
| `tests/test_tmag5273.cpp`         | TMAG5273 begin, readXYZ, null mux handling                                  |
| `tests/test_euler_conversion.cpp` | quat→Euler (5 cases), SlidingWindow (5 cases), FeatureNormalizer (5 cases) |

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
| 4 | I2C scanner hanging (ESP-IDF blocking on NACK) | Reduced scan to minimal address tests (0x70, 0x4A, 0x22) |
| 5 | Build error: braces around scalar initializer for float | Removed `PROGMEM`, used double brace syntax `{{...}, ...}` for GestureSignature array |

### Hardware Configuration Update (2026-05-21)

- **Pull-ups changed**: Main bus 5.1kΩ → **2kΩ** (closer to SOP 2.2kΩ spec, faster rise time at 400kHz)
- **Channel fix**: Firmware `MuxChannels::BNO085_IMU` changed from 7 → **5** to match hardware wiring (SD5/SC5)
- **Ch5 sub-bus pull-ups**: Installed **5.1kΩ** on SD5/SC5→3.3V for GY-BNO085
- **Hardware clarification**: Using **PCA9548A** (NXP, register-compatible with TCA9548A) and **GY-BNO085** (Adafruit breakout, likely has onboard pull-ups)
- **Ch0-4 sub-bus pull-ups**: Not installed (TMAG5273s not arrived). Main bus 2kΩ passes through PCA9548A internal switches — sufficient for testing.

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

### Hardware Issue — UNRESOLVED

I2C devices (TCA9548A at 0x70, BNO085 at 0x4A) return NACK. Suspected causes:
- Devices may not be powered (no LEDs visible)
- Wiring not connected properly
- ~~5.1kΩ pull-ups~~ → Fixed: now 2kΩ (appropriate for 400kHz Fast Mode at 3.3V)
- ~~Channel mismatch (code=7, hardware=5)~~ → Fixed: code changed to channel 5

**Next action**: Follow DM40B multimeter wiring debug guide to verify physical connections

---

## Active Work

**Current**: Phase 3 — L1 Edge Inference (Edge Impulse MVP Path A)

Simulation mode is operational. Can proceed with Edge Impulse data collection using synthetic data while hardware debugging continues in parallel.

### Priority: Path A — Edge Impulse MVP (快速验证)

Per SOP §6.1, ESP32 CSV output already compatible with `edge-impulse-data-forwarder`. Steps:

1. ✅ Install edge-impulse-cli: `npm install -g edge-impulse-cli`
2. ✅ Firmware outputs CSV in simulation mode (20 gesture classes)
3. **→ NEXT**: Start data forwarder: `edge-impulse-data-forwarder --baud-rate 115200 --frequency 100`
4. Collect labeled gesture data in Edge Impulse Studio (30-60 samples per gesture)
5. Train 1D-CNN classifier (200 epochs, lr=0.001)
6. Export as Arduino Library → integrate via PlatformIO `lib_deps`

**Target**: 2-3 days to MVP verification (simulation data available immediately)

Path B (PyTorch → TFLite INT8) deferred to Phase 3.5 Benchmark.

### Parallel Work Tracks

| Track | Status | Blocking? |
|-------|--------|-----------|
| Edge Impulse data collection (simulation) | Ready to start | **No** — synthetic data available |
| Hardware verification (DM40B multimeter) | User action needed | **No** — simulation mode unblocks firmware |
| TMAG5273 sensor installation | Awaiting delivery | **No** — reserved in simulation mode |

### Phase Status Summary

| Phase | Name | Status |
|-------|------|--------|
| P0 | Project init | Done |
| P1 | HAL & drivers | Done |
| P2 | Signal processing | Done |
| P3 | L1 Edge Inference — Simulation mode | **← ACTIVE** |
| P3.5 | Model Benchmark | Pending |
| P4 | Communication (BLE/UDP/Protobuf) | Scaffold exists |
| P5 | Python Relay + L2 ST-GCN | Scaffold exists |
| P6 | Web rendering / Unity Pro | Scaffold exists |
| P7 | Integration testing | Pending |