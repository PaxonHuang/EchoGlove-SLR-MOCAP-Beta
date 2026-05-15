# PROGRESS.md — Cross-Session State Tracker

**Last updated**: 2026-05-15

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

## Active Work

**Next**: Phase 3 — L1 Edge Inference (TinyML / TFLite Micro)

### Priority: Path A — Edge Impulse MVP (快速验证)

Per SOP §6.1, ESP32 CSV output already compatible with `edge-impulse-data-forwarder`. Steps:

1. Install edge-impulse-cli: `npm install -g edge-impulse-cli`
2. Start data forwarder: `edge-impulse-data-forwarder`
3. Collect labeled gesture data in Edge Impulse Studio
4. Train 1D-CNN classifier (200 epochs, lr=0.001)
5. Export as Arduino Library → integrate via PlatformIO `lib_deps`

**Target**: 2-3 days to MVP verification

Path B (PyTorch → TFLite INT8) deferred to Phase 3.5 Benchmark.

### Phase Status Summary

| Phase | Name | Status |
|-------|------|--------|
| P0 | Project init | Done |
| P1 | HAL & drivers | Done |
| P2 | Signal processing | Done |
| P3 | L1 Edge Inference | **← NEXT** |
| P3.5 | Model Benchmark | Pending |
| P4 | Communication (BLE/UDP/Protobuf) | Scaffold exists |
| P5 | Python Relay + L2 ST-GCN | Scaffold exists |
| P6 | Web rendering / Unity Pro | Scaffold exists |
| P7 | Integration testing | Pending |