# Phase 1 + Phase 2 Design: FreeRTOS Wiring & Signal Processing

**Date:** 2026-04-30
**Scope:** ESP32 firmware core — complete Phase 1 task wiring and Phase 2 signal processing
**Approach:** Minimal Integration (Approach A) — wire up existing code, add RingBuffer + Normalizer + CSV

---

## 1. Phase 1: FreeRTOS Task Wiring

### 1.1 Setup — Enable SensorManager Init

Uncomment and call `g_sensor_manager.begin()` in `setup()`. This activates I2C bus init, mux scan, Hall sensor init, and BNO085 init.

### 1.2 Task_SensorRead (Core 1, Priority 3, 100Hz)

Real implementation — replaces current TODO stub:

1. Call `g_sensor_manager.readAll(data)` — reads all sensors with Kalman filtering
2. Push `SensorData` (Kalman-filtered, unnormalized) to `g_data_queue`
3. Push `SensorData` to `g_ring_buffer`
4. If ring buffer is newly full, notify `Task_Inference` via `xTaskNotifyGive()`
5. Print CSV line every 10th frame (normalized values for Edge Impulse compatibility)
6. Timing diagnostics: print actual frame time every 100 frames

**Normalization note:** SensorData pushed to queue and ring buffer contains
Kalman-filtered raw values. Normalization is applied only at two points:
(a) CSV output (for Edge Impulse data collection), and (b) `copyOutNormalized()`
when inference reads from ring buffer. This keeps raw data intact for relay
transmission while providing normalized data for model input.

### 1.3 Task_Inference (Core 0, Priority 2, ~30Hz)

Remains a stub for Phase 3, but reads from ring buffer when notified:

1. Wait for notification from `Task_SensorRead` (ulTaskNotifyTake)
2. Copy 30 frames from ring buffer to flat feature array
3. Placeholder: print inference timing, no actual model yet
4. Push placeholder `GestureResult` to `g_inference_queue`

### 1.4 Task_Comms (Core 0, Priority 1, 100Hz/20Hz)

Remains a stub for Phase 4:

1. Pull from `g_data_queue` and `g_inference_queue` (non-blocking)
2. Placeholder: count frames received, no actual BLE/UDP yet

### 1.5 Bug Fixes in SensorManager

- Fix `TCA9548A_DEFAULT_ADDR` → `TCA9548A::DEFAULT_ADDR` in constructor
- Add `_imu_ok = false` initialization in constructor

---

## 2. RingBuffer (Sliding Window)

### 2.1 Interface

**File:** `glove_firmware/lib/Filters/RingBuffer.h` (header-only)

```cpp
template<uint16_t Capacity = WINDOW_SIZE>
class RingBuffer {
public:
    RingBuffer();
    bool push(const SensorData& frame);       // O(1), overwrites oldest when full
    bool isFull() const;                       // true after Capacity pushes
    uint16_t count() const;                    // frames currently stored
    void reset();                              // clear buffer

    // Copy all frames in chronological order to flat float array
    // Applies Normalizer::normalize() to each frame's 21-dim feature vector
    // Output buffer size must be >= Capacity * FEATURE_COUNT
    void copyOutNormalized(float* out) const;
private:
    SensorData _buf[Capacity];
    uint16_t   _head;    // next write position
    uint16_t   _count;   // total frames pushed (caps at Capacity)
};
```

### 2.2 Memory

30 frames × ~180 bytes ≈ 5.4 KB. Fits in ESP32-S3 SRAM.

### 2.3 Synchronization

Single-writer (Task_SensorRead on Core 1), single-reader (Task_Inference on Core 0).
Synchronization via FreeRTOS task notification — no mutex needed. Task_SensorRead
calls `xTaskNotifyGive(TaskInferenceHandle)` when `isFull()` first becomes true.
Task_Inference blocks on `ulTaskNotifyTake()`.

---

## 3. Normalizer (Min-Max Static Calibration)

### 3.1 Interface

**File:** `glove_firmware/lib/Filters/Normalizer.h` (header-only)

```cpp
class Normalizer {
public:
    // Normalize 21-dim feature array from raw filtered values to [-1, 1]
    static void normalize(const float* raw, float* out);

    // Per-channel min/max constants (hard-coded from SOP estimates)
    // Hall: 15 channels, range ±40 mT → [-1, 1]
    // Hall ch0-14: 5 sensors × 3 axes (thumb_x, thumb_y, thumb_z, ..., pinky_z)
    // Euler: 3 channels, range ±180° → [-1, 1]
    // Gyro: 3 channels, range ±500°/s → [-1, 1]
    static constexpr float CHANNEL_MIN[FEATURE_COUNT] = {
        -40,-40,-40, -40,-40,-40, -40,-40,-40, -40,-40,-40, -40,-40,-40,  // Hall
        -180,-180,-180,  // Euler
        -500,-500,-500   // Gyro
    };
    static constexpr float CHANNEL_MAX[FEATURE_COUNT] = {
        40,40,40, 40,40,40, 40,40,40, 40,40,40, 40,40,40,  // Hall
        180,180,180,  // Euler
        500,500,500   // Gyro
    };
};
```

### 3.2 Normalization Formula

All 21 channels normalized to **[-1, 1]** uniformly:

```
x_norm = 2 * (x - min) / (max - min) - 1
```

Per-channel ranges:
- **Hall X/Y/Z (15 channels):** min = -40 mT, max = 40 mT (±40 mT TMAG5273 range)
- **Euler Roll/Pitch/Yaw (3 channels):** min = -180°, max = 180°
- **Gyro X/Y/Z (3 channels):** min = -500°/s, max = 500°/s

### 3.3 Calibration Path

Static constants are initial estimates. Once real calibration data is collected,
replace the hard-coded values with measured per-channel min/max. The Normalizer
interface stays the same — only the constant arrays change.

---

## 4. CSV Serial Output (Edge Impulse Compatible)

### 4.1 Format

One line per frame (throttled to every 10th frame):

```
0.12,-0.34,0.56,...,-0.91,0.45\n
```

21 comma-separated float values, normalized to [-1, 1]. No label field (unlabeled
data for Edge Impulse ingestion).

### 4.2 Header Line

Printed once at Task_SensorRead startup:

```
hall_x0,hall_y0,hall_z0,hall_x1,...,euler_roll,euler_pitch,euler_yaw,gyro_x,gyro_y,gyro_z
```

### 4.3 Throttling

Configurable via `#define SERIAL_CSV_EVERY_N 10`. At 100Hz sensor rate, this
produces 10 lines/sec ≈ 1000 chars/sec, well within 115200 baud capacity.

### 4.4 Edge Impulse Integration

User runs `edge-impulse-data-forwarder` on host PC, pointing to the ESP32 serial
port. The forwarder auto-detects the CSV format and streams to Edge Impulse cloud.

---

## 5. Build Verification

### 5.1 Fixes Required

1. **SensorManager constructor:** `TCA9548A_DEFAULT_ADDR` → `TCA9548A::DEFAULT_ADDR`
2. **SensorManager constructor:** Add `_imu_ok = false` initialization
3. **Stub .cpp files:** Add `RingBuffer.cpp` and `Normalizer.cpp` as compilation
   unit placeholders (PlatformIO library discovery requirement)

### 5.2 Build Command

```powershell
cd glove_firmware
pio run
```

Uses PowerShell on Windows (bash produces no output per CLAUDE.md).

---

## 6. File Change Summary

| File | Action |
|------|--------|
| `glove_firmware/src/main.cpp` | Rewrite: fill task stubs, add ring buffer, normalizer, CSV |
| `glove_firmware/lib/Sensors/SensorManager.h` | Fix: TCA9548A_DEFAULT_ADDR, add _imu_ok init |
| `glove_firmware/lib/Filters/RingBuffer.h` | New: ring buffer for sliding window |
| `glove_firmware/lib/Filters/RingBuffer.cpp` | New: stub for PlatformIO discovery |
| `glove_firmware/lib/Filters/Normalizer.h` | New: Min-Max normalization |
| `glove_firmware/lib/Filters/Normalizer.cpp` | New: stub for PlatformIO discovery |
| `glove_firmware/lib/Filters/Filters.h` | Update: add includes for RingBuffer + Normalizer |

Total new code: ~300 lines. No architectural changes beyond what the SOP specifies.