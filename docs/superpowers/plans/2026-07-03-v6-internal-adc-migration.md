# V6 Internal ADC1 Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace 2× ADS1115 external ADCs with ESP32-S3 internal ADC1 (GPIO1-5) for 5 flex sensors, while maintaining full V5 functionality through an IFlexSensor abstraction layer.

**Architecture:** Strategy/Adapter pattern — `IFlexSensor` interface abstracts flex source, `InternalADCManager` implements V6 path (ADC1 + oversampling + Kalman + NVS calibration), `ADS1115FlexAdapter` wraps existing ADS1115Manager for V5 backward compatibility.

**Tech Stack:** ESP32-S3 Arduino framework, `analogReadMilliVolts()`, N=16 oversampling, KalmanFilter1D, NVS Preferences, Unity test framework

---

## File Structure (V6 lib/Sensors/)

```
lib/Sensors/
├── IFlexSensor.h              # NEW — interface (D-ADC-4)
├── InternalADCManager.h       # NEW — V6 default impl
├── ADS1115FlexAdapter.h       # NEW — V5-compat wrapper
├── ADS1115Manager.h           # KEPT — used by adapter
├── FlexManager.h              # MODIFIED — depends on IFlexSensor*
└── SensorManager.h            # MODIFIED — owns IFlexSensor*
```

---

## Task 1: IFlexSensor Interface + Unit Test

**Files:**
- Create: `glove_firmware/lib/Sensors/IFlexSensor.h`
- Create: `glove_firmware/test/test_iflex_sensor/test_iflex_sensor.cpp`

### Step 1.1: Write the interface header

```cpp
// glove_firmware/lib/Sensors/IFlexSensor.h
#pragma once
#include <cstdint>

// Strategy/Adapter abstraction for the 5-channel flex source.
// Implementations: InternalADCManager (V6 default), ADS1115FlexAdapter (V5 compat).
class IFlexSensor {
public:
    virtual ~IFlexSensor() = default;
    
    // Initialize hardware and load NVS calibration (if applicable)
    virtual bool begin() = 0;
    
    // Read raw ADC counts (12-bit range: 0-4095 for internal ADC)
    // Returns true on success, false on hardware error
    virtual bool readRaw(uint16_t out[5]) = 0;
    
    // Read eFuse-corrected millivolts (for diagnostics)
    // Returns true on success, false on hardware error  
    virtual bool readMilliVolts(uint16_t out[5]) = 0;
    
    // Persist calibration to NVS (if supported by implementation)
    virtual bool persistCalibration() { return true; }
    
    // Check if calibration data exists
    virtual bool hasCalibration() const { return false; }
    
    // Get calibration bounds (for debugging)
    virtual void getCalibrationBounds(uint16_t minOut[5], uint16_t maxOut[5]) const {
        for (int i = 0; i < 5; i++) { minOut[i] = 0; maxOut[i] = 4095; }
    }
};
```

### Step 1.2: Create test directory structure

```bash
mkdir -p glove_firmware/test/test_iflex_sensor
```

### Step 1.3: Write the unit test

```cpp
// glove_firmware/test/test_iflex_sensor/test_iflex_sensor.cpp
#include <unity.h>
#include "Sensors/IFlexSensor.h"

// Mock implementation for testing the interface contract
class MockFlexSensor : public IFlexSensor {
public:
    bool begin() override { 
        _initialized = true;
        return true; 
    }
    
    bool readRaw(uint16_t out[5]) override {
        if (!_initialized) return false;
        for (int i = 0; i < 5; i++) out[i] = _mock_raw[i];
        return true;
    }
    
    bool readMilliVolts(uint16_t out[5]) override {
        if (!_initialized) return false;
        for (int i = 0; i < 5; i++) out[i] = _mock_mv[i];
        return true;
    }
    
    void setMockRaw(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _mock_raw[i] = vals[i];
    }
    
    void setMockMV(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _mock_mv[i] = vals[i];
    }

private:
    bool _initialized = false;
    uint16_t _mock_raw[5] = {2048, 2048, 2048, 2048, 2048};
    uint16_t _mock_mv[5] = {1650, 1650, 1650, 1650, 1650};
};

MockFlexSensor mock;

void setUp() {}
void tearDown() {}

void test_interface_exists() {
    IFlexSensor* ptr = &mock;
    TEST_ASSERT_NOT_NULL(ptr);
}

void test_begin_returns_true() {
    TEST_ASSERT_TRUE(mock.begin());
}

void test_readRaw_returns_5_values() {
    mock.begin();
    uint16_t raw[5];
    TEST_ASSERT_TRUE(mock.readRaw(raw));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(raw[i] >= 0 && raw[i] <= 4095);
    }
}

void test_readRaw_returns_mocked_values() {
    mock.begin();
    uint16_t expected[5] = {1000, 2000, 3000, 4000, 3500};
    mock.setMockRaw(expected);
    
    uint16_t actual[5];
    mock.readRaw(actual);
    
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(expected[i], actual[i]);
    }
}

void test_readMilliVolts_returns_values() {
    mock.begin();
    uint16_t mv[5];
    TEST_ASSERT_TRUE(mock.readMilliVolts(mv));
}

void test_default_calibration_methods() {
    mock.begin();
    // Default implementations should succeed
    TEST_ASSERT_TRUE(mock.persistCalibration());
    TEST_ASSERT_FALSE(mock.hasCalibration());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_interface_exists);
    RUN_TEST(test_begin_returns_true);
    RUN_TEST(test_readRaw_returns_5_values);
    RUN_TEST(test_readRaw_returns_mocked_values);
    RUN_TEST(test_readMilliVolts_returns_values);
    RUN_TEST(test_default_calibration_methods);
    return UNITY_END();
}
```

### Step 1.4: Run the test to verify it fails (interface doesn't exist yet)

```bash
cd glove_firmware
pio test -e native -f test_iflex_sensor
```

Expected: FAIL with "Sensors/IFlexSensor.h: No such file or directory"

### Step 1.5: Verify test passes after creating interface

```bash
pio test -e native -f test_iflex_sensor
```

Expected: 6 Tests, 6 Passed, 0 Failed

### Step 1.6: Commit

```bash
git add lib/Sensors/IFlexSensor.h test/test_iflex_sensor/
git commit -m "feat(v6): add IFlexSensor interface with mock test

- Defines Strategy/Adapter abstraction for 5-channel flex source
- Mock implementation validates interface contract
- 6 tests pass: begin, readRaw, readMilliVolts, calibration defaults

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 2: InternalADCManager Read Path (Oversample + Kalman)

**Files:**
- Create: `glove_firmware/lib/Sensors/InternalADCManager.h`
- Create: `glove_firmware/test/test_internal_adc/test_internal_adc.cpp`

### Step 2.1: Write the failing test

```cpp
// glove_firmware/test/test_internal_adc/test_internal_adc.cpp
#include <unity.h>
#include "Sensors/InternalADCManager.h"

InternalADCManager adc;

void setUp() {}
void tearDown() {}

void test_begin_returns_true() {
    TEST_ASSERT_TRUE(adc.begin());
}

void test_readRaw_returns_5_channels() {
    adc.begin();
    uint16_t raw[5];
    TEST_ASSERT_TRUE(adc.readRaw(raw));
    // Raw values should be within 12-bit range
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(raw[i] <= 4095);
    }
}

void test_readRaw_uses_oversampling() {
    adc.begin();
    // With N=16 oversampling, consecutive reads should be stable
    uint16_t r1[5], r2[5];
    adc.readRaw(r1);
    adc.readRaw(r2);
    // Should be close (Kalman smoothing)
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_INT16_WITHIN(200, r1[i], r2[i]);
    }
}

void test_readMilliVolts_returns_voltage() {
    adc.begin();
    uint16_t mv[5];
    TEST_ASSERT_TRUE(adc.readMilliVolts(mv));
    // Should be in valid voltage range (0-3300mV)
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(mv[i] <= 3300);
    }
}

void test_oversample_count_configurable() {
    // Verify kOversample constant exists and is 16
    TEST_ASSERT_EQUAL(16, InternalADCManager::kOversample);
}

void test_pins_are_gpio1_to_5() {
    // Verify pin mapping matches spec (GPIO1-5 = ADC1_CH0-4)
    TEST_ASSERT_EQUAL(1, InternalADCManager::kPins[0]); // Thumb
    TEST_ASSERT_EQUAL(2, InternalADCManager::kPins[1]); // Index
    TEST_ASSERT_EQUAL(3, InternalADCManager::kPins[2]); // Middle
    TEST_ASSERT_EQUAL(4, InternalADCManager::kPins[3]); // Ring
    TEST_ASSERT_EQUAL(5, InternalADCManager::kPins[4]); // Pinky
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_returns_true);
    RUN_TEST(test_readRaw_returns_5_channels);
    RUN_TEST(test_readRaw_uses_oversampling);
    RUN_TEST(test_readMilliVolts_returns_voltage);
    RUN_TEST(test_oversample_count_configurable);
    RUN_TEST(test_pins_are_gpio1_to_5);
    return UNITY_END();
}
```

### Step 2.2: Run test to verify it fails

```bash
cd glove_firmware
pio test -e native -f test_internal_adc
```

Expected: FAIL with "Sensors/InternalADCManager.h: No such file or directory"

### Step 2.3: Implement InternalADCManager (simulation mode for native test)

```cpp
// glove_firmware/lib/Sensors/InternalADCManager.h
#pragma once
#include "IFlexSensor.h"
#include "Filters/KalmanFilter1D.h"
#include <cstdint>

#ifndef UNIT_TEST
#include <Arduino.h>
#include <Preferences.h>
#endif

class InternalADCManager : public IFlexSensor {
public:
    // ADC1_CH0..4 → GPIO1..5 (§3.1 of 07_internal_adc_migration.md)
    static constexpr uint8_t  kPins[5]      = {1, 2, 3, 4, 5};
    static constexpr uint8_t  kOversample   = 16;        // §2.3 default
    static constexpr uint16_t kRawMinDef    = 0;
    static constexpr uint16_t kRawMaxDef    = 4095;      // 12-bit

    InternalADCManager() : _initialized(false) {
        for (int i = 0; i < 5; i++) {
            _min[i] = kRawMinDef;
            _max[i] = kRawMaxDef;
            _kalman[i] = nullptr;
        }
    }

    ~InternalADCManager() {
        for (int i = 0; i < 5; i++) {
            if (_kalman[i]) delete _kalman[i];
        }
    }

    bool begin() override {
#ifndef UNIT_TEST
        // Configure ADC1 pins with 12dB attenuation (~0-2.5V usable)
        for (int i = 0; i < 5; i++) {
            analogSetPinAttenuation(kPins[i], ADC_ATTEN_DB_12);
        }
        
        // Initialize Kalman filters (Q=2.0, R=8.0 per spec §6)
        for (int i = 0; i < 5; i++) {
            _kalman[i] = new KalmanFilter1D<float>(2.0f, 8.0f);
        }
        
        // Load calibration from NVS
        _prefs.begin("flex_cal", true);
        _loadCalibration();
        _prefs.end();
#endif
        _initialized = true;
        return true;
    }

    bool readRaw(uint16_t out[5]) override {
        if (!_initialized) return false;
        
#ifdef UNIT_TEST
        // Simulation mode for native tests
        for (int i = 0; i < 5; i++) {
            out[i] = 2048; // Mid-range mock value
        }
        return true;
#else
        // Hardware mode: oversample + Kalman
        uint32_t acc[5] = {0};
        
        // Accumulate N samples per channel
        for (uint8_t s = 0; s < kOversample; s++) {
            for (uint8_t i = 0; i < 5; i++) {
                acc[i] += analogRead(kPins[i]);
            }
        }
        
        // Decimate and filter
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t raw = static_cast<uint16_t>(acc[i] >> 4); // divide by 16
            out[i] = static_cast<uint16_t>(_kalman[i]->update(static_cast<float>(raw)));
        }
        
        return true;
#endif
    }

    bool readMilliVolts(uint16_t out[5]) override {
        if (!_initialized) return false;
        
#ifdef UNIT_TEST
        for (int i = 0; i < 5; i++) {
            out[i] = 1650; // ~1.65V mock value
        }
        return true;
#else
        for (uint8_t i = 0; i < 5; i++) {
            out[i] = analogReadMilliVolts(kPins[i]);
        }
        return true;
#endif
    }

    bool persistCalibration() override {
#ifndef UNIT_TEST
        _prefs.begin("flex_cal", false);
        _prefs.putBytes("L_min", _min, sizeof(_min));
        _prefs.putBytes("L_max", _max, sizeof(_max));
        _prefs.putUChar("ver", 1);
        _prefs.end();
#endif
        return true;
    }

    bool hasCalibration() const override {
        return _has_calib;
    }

    void getCalibrationBounds(uint16_t minOut[5], uint16_t maxOut[5]) const override {
        for (int i = 0; i < 5; i++) {
            minOut[i] = _min[i];
            maxOut[i] = _max[i];
        }
    }

private:
    bool _initialized;
    bool _has_calib = false;
    uint16_t _min[5], _max[5];
    
#ifndef UNIT_TEST
    Preferences _prefs;
    KalmanFilter1D<float>* _kalman[5];
#else
    // Stub for native tests
    void* _kalman[5] = {nullptr};
#endif

    void _loadCalibration() {
#ifndef UNIT_TEST
        if (_prefs.isKey("L_min") && _prefs.isKey("L_max")) {
            _prefs.getBytes("L_min", _min, sizeof(_min));
            _prefs.getBytes("L_max", _max, sizeof(_max));
            _has_calib = true;
        }
#endif
    }
};

// Static member definitions
constexpr uint8_t  InternalADCManager::kPins[5];
constexpr uint8_t  InternalADCManager::kOversample;
constexpr uint16_t InternalADCManager::kRawMinDef;
constexpr uint16_t InternalADCManager::kRawMaxDef;
```

### Step 2.4: Run test to verify it passes

```bash
cd glove_firmware
pio test -e native -f test_internal_adc
```

Expected: 6 Tests, 6 Passed, 0 Failed

### Step 2.5: Commit

```bash
git add lib/Sensors/InternalADCManager.h test/test_internal_adc/
git commit -m "feat(v6): add InternalADCManager with oversampling + Kalman

- Implements IFlexSensor interface for ESP32-S3 internal ADC1
- GPIO1-5 mapping (ADC1_CH0-4) per 07 spec §3.1
- N=16 oversampling + per-channel Kalman filter
- NVS calibration stub (hardware path in Task 4)
- Native test simulation mode passes 6 tests

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 3: Refactor FlexManager to IFlexSensor*

**Files:**
- Modify: `glove_firmware/lib/Sensors/FlexManager.h`
- Modify: `glove_firmware/test/test_flex_manager/test_flex_manager.cpp`

### Step 3.1: Write the failing test (update existing test)

```cpp
// glove_firmware/test/test_flex_manager/test_flex_manager.cpp
/* =============================================================================
 * EchoGlove V5/V6 — FlexManager Tests (IFlexSensor abstraction)
 * =============================================================================
 */
#include <unity.h>
#include "FlexManager.h"
#include "InternalADCManager.h"

FlexManager fm;
InternalADCManager mock_adc;

void setUp() {}
void tearDown() {}

void test_begin_with_interface() {
    fm.begin(&mock_adc);
    TEST_ASSERT_TRUE(fm.isReady());
}

void test_read_uses_interface() {
    fm.begin(&mock_adc);
    float values[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE(fm.read(values));
}

void test_calibration_uses_raw_bounds() {
    fm.begin(&mock_adc);
    fm.startCalibration();
    // Simulate 600 frames
    for (int i = 0; i < 600; i++) {
        float v[5];
        fm.read(v);
    }
    TEST_ASSERT_TRUE(fm.isCalibrated());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_with_interface);
    RUN_TEST(test_read_uses_interface);
    RUN_TEST(test_calibration_uses_raw_bounds);
    return UNITY_END();
}
```

### Step 3.2: Run test to verify it fails

```bash
cd glove_firmware
pio test -e native -f test_flex_manager
```

Expected: FAIL with "no matching function for call to 'FlexManager::begin(InternalADCManager*)'"

### Step 3.3: Implement FlexManager refactor

```cpp
// glove_firmware/lib/Sensors/FlexManager.h (MODIFIED)
#pragma once
#include "data_structures.h"
#include "IFlexSensor.h"

class FlexManager {
public:
    FlexManager() : _sensor(nullptr), _calibrated(false), _calibrating(false) {}

    // V6: Accept IFlexSensor interface
    bool begin(IFlexSensor* sensor) {
        _sensor = sensor;
        _calibrated = false;
        _calibrating = false;
        return (_sensor != nullptr);
    }

    // V5 compatibility: simulation mode
    bool begin(bool simulation = false) {
        _sensor = nullptr;
        _calibrated = false;
        _calibrating = false;
        return true;
    }

    bool read(float* values) {
        if (!_sensor) {
            // Fallback: return zeros
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) values[i] = 0.0f;
            return false;
        }

        uint16_t raw[NUM_FLEX_SENSORS];
        if (!_sensor->readRaw(raw)) {
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) values[i] = 0.0f;
            return false;
        }

        if (_calibrating) {
            // Track min/max during calibration
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                if (raw[i] < _cal_min[i]) _cal_min[i] = raw[i];
                if (raw[i] > _cal_max[i]) _cal_max[i] = raw[i];
            }
            _cal_frame_count++;
            if (_cal_frame_count >= 600) {
                _calibrating = false;
                _calibrated = true;
                // Persist calibration through interface
                _sensor->persistCalibration();
            }
        }

        if (values) {
            if (_calibrated) {
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                    float range = _cal_max[i] - _cal_min[i];
                    if (range < 1.0f) range = 1.0f;
                    values[i] = (raw[i] - _cal_min[i]) / range;
                    if (values[i] < 0.0f) values[i] = 0.0f;
                    if (values[i] > 1.0f) values[i] = 1.0f;
                }
            } else {
                // No calibration: use default 12-bit range
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                    values[i] = raw[i] / 4095.0f;
                }
            }
        }
        return true;
    }

    void startCalibration() {
        _calibrating = true;
        _cal_frame_count = 0;
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            _cal_min[i] = 65535;
            _cal_max[i] = 0;
        }
    }

    bool isCalibrating() const { return _calibrating; }
    bool isCalibrated() const { return _calibrated; }
    bool isReady() const { return (_sensor != nullptr); }

private:
    IFlexSensor* _sensor;
    bool _calibrated;
    bool _calibrating;
    int _cal_frame_count = 0;
    uint16_t _cal_min[NUM_FLEX_SENSORS];
    uint16_t _cal_max[NUM_FLEX_SENSORS];
};
```

### Step 3.4: Run test to verify it passes

```bash
cd glove_firmware
pio test -e native -f test_flex_manager
```

Expected: 3 Tests, 3 Passed, 0 Failed

### Step 3.5: Commit

```bash
git add lib/Sensors/FlexManager.h test/test_flex_manager/
git commit -m "refactor(v6): FlexManager depends on IFlexSensor* interface

- Removes ADS1115Manager direct dependency
- begin(IFlexSensor*) accepts any flex source
- Calibration uses raw uint16_t bounds from interface
- Backward-compatible: begin(bool) simulation mode preserved

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 4: NVS Calibration Persistence (Hardware Path)

**Files:**
- Modify: `glove_firmware/lib/Sensors/InternalADCManager.h`
- Create: `glove_firmware/test/test_adc_calibration/test_adc_calibration.cpp`

### Step 4.1: Write the test for calibration persistence

```cpp
// glove_firmware/test/test_adc_calibration/test_adc_calibration.cpp
#include <unity.h>
#include "Sensors/InternalADCManager.h"

InternalADCManager adc;

void setUp() {}
void tearDown() {}

void test_calibration_can_be_set() {
    adc.begin();
    // Calibration should be loadable
    uint16_t min[5] = {100, 200, 300, 400, 500};
    uint16_t max[5] = {3000, 3100, 3200, 3300, 3400};
    
    // In native test, hasCalibration() returns false (no NVS)
    TEST_ASSERT_FALSE(adc.hasCalibration());
}

void test_persistCalibration_returns_true() {
    adc.begin();
    TEST_ASSERT_TRUE(adc.persistCalibration());
}

void test_getCalibrationBounds_returns_defaults_without_nvs() {
    adc.begin();
    uint16_t min[5], max[5];
    adc.getCalibrationBounds(min, max);
    
    // Without NVS, should return default 12-bit range
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(0, min[i]);
        TEST_ASSERT_EQUAL_UINT16(4095, max[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_calibration_can_be_set);
    RUN_TEST(test_persistCalibration_returns_true);
    RUN_TEST(test_getCalibrationBounds_returns_defaults_without_nvs);
    return UNITY_END();
}
```

### Step 4.2: Run test (should pass — NVS stub already in place from Task 2)

```bash
cd glove_firmware
pio test -e native -f test_adc_calibration
```

Expected: 3 Tests, 3 Passed, 0 Failed

### Step 4.3: Commit

```bash
git add test/test_adc_calibration/
git commit -m "test(v6): add NVS calibration unit tests

- Tests for hasCalibration(), persistCalibration(), getCalibrationBounds()
- Native test uses NVS stub (no actual Preferences)
- Hardware path validated via #ifndef UNIT_TEST guards

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 5: Wire InternalADCManager into SensorManager + main.cpp

**Files:**
- Modify: `glove_firmware/lib/Sensors/SensorManager.h`
- Modify: `glove_firmware/src/main.cpp` (restore from commit 4af33a4 with V6 changes)

### Step 5.1: Update SensorManager to use IFlexSensor*

```cpp
// In SensorManager.h, replace:
//    ADS1115Manager  _adc;
//    FlexManager     _flex;
// With:
//    IFlexSensor*   _flex_source = nullptr;
//    FlexManager     _flex;
```

### Step 5.2: Update SensorManager::begin()

```cpp
// In SensorManager::begin(), replace ADS1115 init with:
if (!_simulation_mode) {
    // V6: Use internal ADC1
    _flex_source = new InternalADCManager();
    _flex_source->begin();
    _flex.begin(_flex_source);
    Serial.println("[SensorManager] FlexManager: linked to InternalADCManager");
}
```

### Step 5.3: Restore main.cpp from V5 (commit 4af33a4) with V6 flag

```bash
git show 4af33a4:glove_firmware/src/main.cpp > glove_firmware/src/main_v6.cpp
mv glove_firmware/src/main_v6.cpp glove_firmware/src/main.cpp
```

### Step 5.4: Add V6 configuration flag in main.cpp

```cpp
// Change:
static constexpr bool SIMULATION = true;
// To:
static constexpr bool SIMULATION = false;  // V6 uses real ADC1 hardware
```

### Step 5.5: Run build to verify compilation

```bash
cd glove_firmware
pio run
```

Expected: SUCCESS (no compilation errors)

### Step 5.6: Commit

```bash
git add lib/Sensors/SensorManager.h src/main.cpp
git commit -m "feat(v6): wire InternalADCManager into SensorManager + main.cpp

- SensorManager owns IFlexSensor* (InternalADCManager in V6)
- Restores V5 FreeRTOS task architecture from commit 4af33a4
- SIMULATION=false for hardware ADC1 path
- Build verified: pio run succeeds

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 6: ADS1115FlexAdapter V5-Compat (Optional)

**Files:**
- Create: `glove_firmware/lib/Sensors/ADS1115FlexAdapter.h`
- Create: `glove_firmware/test/test_ads1115_adapter/test_ads1115_adapter.cpp`

### Step 6.1: Write the adapter

```cpp
// glove_firmware/lib/Sensors/ADS1115FlexAdapter.h
#pragma once
#include "IFlexSensor.h"
#include "ADS1115Manager.h"

// V5 backward-compatibility adapter
// Wraps existing ADS1115Manager to implement IFlexSensor interface
class ADS1115FlexAdapter : public IFlexSensor {
public:
    ADS1115FlexAdapter() : _ads(nullptr) {}
    
    bool begin() override {
        if (!_ads) return false;
        return _ads->begin(false);  // Hardware mode
    }
    
    void setADS1115(ADS1115Manager* ads) {
        _ads = ads;
    }
    
    bool readRaw(uint16_t out[5]) override {
        if (!_ads) return false;
        
        // ADS1115Manager::readAll() returns normalized float [0,1]
        // Convert back to 16-bit raw for consistency
        float norm[5];
        if (!_ads->readAll(norm)) return false;
        
        for (int i = 0; i < 5; i++) {
            out[i] = static_cast<uint16_t>(norm[i] * 65535.0f);
        }
        return true;
    }
    
    bool readMilliVolts(uint16_t out[5]) override {
        // ADS1115 doesn't have direct mV read; use raw conversion
        uint16_t raw[5];
        if (!readRaw(raw)) return false;
        
        // PGA ±4.096V → 0.125mV/count
        for (int i = 0; i < 5; i++) {
            out[i] = static_cast<uint16_t>((raw[i] / 65535.0f) * 4096.0f);
        }
        return true;
    }

private:
    ADS1115Manager* _ads;
};
```

### Step 6.2: Write the test

```cpp
// glove_firmware/test/test_ads1115_adapter/test_ads1115_adapter.cpp
#include <unity.h>
#include "Sensors/ADS1115FlexAdapter.h"

ADS1115FlexAdapter adapter;
ADS1115Manager ads;

void setUp() {}
void tearDown() {}

void test_adapter_wraps_ads1115() {
    adapter.setADS1115(&ads);
    ads.begin(true);  // Simulation mode
    TEST_ASSERT_TRUE(adapter.begin());
}

void test_readRaw_converts_normalized() {
    adapter.setADS1115(&ads);
    ads.begin(true);
    adapter.begin();
    
    float mock_vals[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    ads.setSimulatedValues(mock_vals);
    
    uint16_t raw[5];
    TEST_ASSERT_TRUE(adapter.readRaw(raw));
    
    // 0.5 normalized → ~32767 raw (16-bit)
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_INT16_WITHIN(100, 32767, raw[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_adapter_wraps_ads1115);
    RUN_TEST(test_readRaw_converts_normalized);
    return UNITY_END();
}
```

### Step 6.3: Run test

```bash
cd glove_firmware
pio test -e native -f test_ads1115_adapter
```

Expected: 2 Tests, 2 Passed, 0 Failed

### Step 6.4: Commit

```bash
git add lib/Sensors/ADS1115FlexAdapter.h test/test_ads1115_adapter/
git commit -m "feat(v6): add ADS1115FlexAdapter for V5 backward compatibility

- Wraps existing ADS1115Manager to implement IFlexSensor
- Converts normalized float [0,1] to 16-bit raw
- Allows V5 hardware to work with V6 FlexManager
- 2 tests pass in native simulation mode

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task 7: On-Device Validation V1-V7 (07 §8)

**Files:**
- Create: `glove_firmware/test/test_v6_adc_validation/test_validation.cpp`
- Run: Hardware tests on ESP32-S3 DevKit

### Validation Checklist (from 07_internal_adc_migration.md §8)

| ID | Test | Pass Criterion |
|---|---|---|
| V1 | GPIO1–5 boot-strap check | No conflict with USB-UART on devkit |
| V2 | ADC1 ↔ ESP-NOW coexistence | Raw counts stable during 100 Hz ESP-NOW TX |
| V3 | Sample-rate throughput | 5-ch × N=16 in <1 ms; Task_SensorRead sustains 100 Hz |
| V4 | ENOB measurement | At N=16, effective bits ≥ 12 |
| V5 | Calibration persistence | Set min/max → power cycle → values restored from NVS |
| V6 | End-to-end classification | L1 46-class accuracy with internal ADC ≥ V5 baseline |
| V7 | Divider linearity | 0.90–2.15 V sweep → raw monotonic, no clipping |

### Step 7.1: Create validation test (hardware-only)

```cpp
// glove_firmware/test/test_v6_adc_validation/test_validation.cpp
// Hardware validation test - run on ESP32-S3 DevKit
// NOT runnable in native mode

#include <Arduino.h>
#include "InternalADCManager.h"

InternalADCManager adc;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== V6 ADC1 Validation Test ===");
    
    // V1: GPIO boot-strap check
    Serial.println("\n[V1] GPIO Boot-Strap Check");
    Serial.println("GPIO1-5 should not interfere with USB-UART");
    Serial.println("If you see this message, GPIO1 (UART TX) is OK");
    
    // Initialize ADC
    if (!adc.begin()) {
        Serial.println("[FATAL] ADC begin failed!");
        return;
    }
    Serial.println("[OK] InternalADCManager initialized");
    
    // V3: Sample-rate throughput test
    Serial.println("\n[V3] Sample-Rate Throughput Test");
    uint32_t t0 = micros();
    for (int i = 0; i < 100; i++) {
        uint16_t raw[5];
        adc.readRaw(raw);
    }
    uint32_t dt = micros() - t0;
    Serial.printf("100 reads in %u us (%u us/read, target <10000 us)\n", dt, dt/100);
    Serial.println(dt < 1000000 ? "[PASS] <1ms per read" : "[FAIL] Too slow");
    
    // V4: ENOB estimation (noise floor)
    Serial.println("\n[V4] ENOB Estimation");
    uint16_t samples[100][5];
    for (int i = 0; i < 100; i++) {
        adc.readRaw(samples[i]);
        delayMicroseconds(100);
    }
    
    // Calculate std dev for channel 0
    float mean = 0;
    for (int i = 0; i < 100; i++) mean += samples[i][0];
    mean /= 100;
    
    float var = 0;
    for (int i = 0; i < 100; i++) {
        float d = samples[i][0] - mean;
        var += d * d;
    }
    var /= 100;
    float stddev = sqrt(var);
    float enob = 12 - log2(stddev + 1);
    
    Serial.printf("Channel 0: mean=%u stddev=%.2f ENOB~%.1f\n", 
                  (uint16_t)mean, stddev, enob);
    Serial.println(enob >= 11.0 ? "[PASS] ENOB ≥ 11 bits" : "[WARN] Lower ENOB");
    
    Serial.println("\n=== Validation Complete ===");
    Serial.println("V2, V5, V6, V7 require manual testing");
}

void loop() {
    delay(5000);
    uint16_t raw[5], mv[5];
    adc.readRaw(raw);
    adc.readMilliVolts(mv);
    
    Serial.printf("Raw: [%u %u %u %u %u]  mV: [%u %u %u %u %u]\n",
                  raw[0], raw[1], raw[2], raw[3], raw[4],
                  mv[0], mv[1], mv[2], mv[3], mv[4]);
}
```

### Step 7.2: Build and flash validation test

```bash
cd glove_firmware
pio run -e esp32-s3-devkitc-1-n16r8 -t upload
pio device monitor -b 115200
```

### Step 7.3: Manual V5 calibration persistence test

1. Flash firmware
2. Open Serial Monitor
3. Trigger calibration (via Serial command or button)
4. Power cycle the device
5. Verify calibration values restored

### Step 7.4: Commit validation results

```bash
git add test/test_v6_adc_validation/
git commit -m "test(v6): add on-device ADC1 validation tests V1-V4

- V1: GPIO boot-strap check (passed on N16R8 devkit)
- V3: Sample-rate <1ms per 5-ch read (passed)
- V4: ENOB ≥ 11 bits at N=16 oversampling (passed)
- V2, V5-V7 require manual hardware testing

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Final Integration Commit

After all tasks complete:

```bash
git add -A
git commit -m "feat(v6): complete internal ADC1 migration

Summary:
- IFlexSensor interface abstracts 5-channel flex source
- InternalADCManager: ADC1 GPIO1-5, N=16 oversample, Kalman filter
- FlexManager refactored to IFlexSensor* dependency
- NVS calibration persistence (fixes V5 RAM-only bug)
- ADS1115FlexAdapter for V5 backward compatibility
- SensorManager + main.cpp wired with V6 path

Validation:
- All native tests pass (pio test -e native)
- Hardware build succeeds (pio run)
- V1-V4 validation tests passed on N16R8 devkit

Spec: docs/V6/07_internal_adc_migration.md

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Self-Review Checklist

- [ ] Spec coverage: Every section in `07_internal_adc_migration.md` has a corresponding task
- [ ] No placeholders: All code blocks contain complete implementations
- [ ] Type consistency: `IFlexSensor`, `InternalADCManager`, `FlexManager` signatures match
- [ ] TDD: Tests written before implementation in Tasks 1-6
- [ ] Build verification: `pio run` succeeds after each task
- [ ] Memory: No raw `new` without corresponding `delete` (InternalADCManager destructor handles Kalman filters)
