# Phase 1 & Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete Phase 1 (HAL 与驱动层开发) and Phase 2 (信号处理与数据采集) for the EdgeAI Data Glove V3 project.

**Architecture:** 
- Phase 1: Hardware Abstraction Layer with TMAG5273, TCA9548A, BNO085 drivers + FreeRTOS dual-core task framework
- Phase 2: Signal processing pipeline with 1D Kalman Filter (15 channels), feature normalization, and sliding window ring buffer

**Tech Stack:** ESP32-S3-DevKitC-1 N16R8, PlatformIO + Arduino, FreeRTOS, I2C, TFLite Micro

---

## Phase 1: HAL 与驱动层开发

### Task 1.1: TCA9548A I2C Multiplexer Driver

**Files:**
- Modify: `glove_firmware/lib/Sensors/TCA9548A.h` (already exists, verify implementation)
- Test: `glove_firmware/tests/test_tca9548a.cpp`

- [ ] **Step 1: Verify TCA9548A driver implementation**

Read and verify the TCA9548A driver has:
1. `disableAll()` method that writes 0x00 to control register
2. `selectChannel(uint8_t ch)` that calls `disableAll()` FIRST, then enables channel
3. 1ms delay after channel selection for bus stability
4. `probeChannel(uint8_t ch, uint8_t devAddr)` for device detection

Current implementation in `TCA9548A.h` already implements this correctly (lines 67-102).

- [ ] **Step 2: Create unit test for TCA9548A**

```cpp
// glove_firmware/tests/test_tca9548a.cpp
#include <unity.h>
#include <Wire.h>
#include "lib/Sensors/TCA9548A.h"

static TCA9548A* mux = nullptr;

void setUp(void) {
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    mux = new TCA9548A(0x70, &Wire);
    TEST_ASSERT_TRUE(mux->begin());
}

void tearDown(void) {
    delete mux;
    mux = nullptr;
}

void test_mux_begin(void) {
    // begin() should return true and disable all channels
    TCA9548A test_mux(0x70, &Wire);
    TEST_ASSERT_TRUE(test_mux.begin());
    TEST_ASSERT_EQUAL(0xFF, test_mux.getActiveChannel());
}

void test_channel_selection(void) {
    for (uint8_t ch = 0; ch < 5; ch++) {
        mux->selectChannel(ch);
        TEST_ASSERT_EQUAL(ch, mux->getActiveChannel());
    }
}

void test_disable_all(void) {
    mux->selectChannel(2);
    mux->disableAll();
    TEST_ASSERT_EQUAL(0xFF, mux->getActiveChannel());
}

void test_probe_device(void) {
    // Probe for TMAG5273 at 0x22 on channel 0
    // This will fail if no hardware connected, so skip in CI
    #ifdef HARDWARE_TEST
    bool found = mux->probeChannel(0, 0x22);
    TEST_ASSERT_TRUE(found);  // Expect Hall sensor on ch0
    #endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mux_begin);
    RUN_TEST(test_channel_selection);
    RUN_TEST(test_disable_all);
    #ifdef HARDWARE_TEST
    RUN_TEST(test_probe_device);
    #endif
    UNITY_END();
}
```

- [ ] **Step 3: Run test and verify it passes**

```powershell
cd glove_firmware
pio test -e native -v
```

Expected: All tests pass (hardware tests skipped in CI mode)

- [ ] **Step 4: Commit**

```bash
git add glove_firmware/lib/Sensors/TCA9548A.h glove_firmware/tests/test_tca9548a.cpp
git commit -m "test(tca9548a): add unit tests for I2C multiplexer driver"
```

---

### Task 1.2: TMAG5273 3D Hall Sensor Driver

**Files:**
- Modify: `glove_firmware/lib/Sensors/TMG5273.h` (exists as TMAG5273.h, verify), `glove_firmware/lib/Sensors/TMG5273.cpp`
- Create: `glove_firmware/lib/Sensors/TMG5273.cpp` (implementation file for header-only driver)
- Test: `glove_firmware/tests/test_tmag5273.cpp`

- [ ] **Step 1: Review TMAG5273 driver**

The header-only driver in `TMAG5273.h` already implements:
1. Device ID verification (0x01)
2. Configuration: 32× averaging, ±40mT range, Set/Reset trigger mode
3. `readXYZ()` with manual trigger and polling for conversion complete
4. Temperature reading via `readTemperature()`
5. Proper I2C mux channel management

The driver is complete but header-only. For better code organization, extract implementation to `.cpp` file.

- [ ] **Step 2: Create TMAG5273.cpp implementation file**

```cpp
// glove_firmware/lib/Sensors/TMG5273.cpp
/* =============================================================================
 * EdgeAI Data Glove V3 — TMAG5273 3D Hall-Effect Sensor Driver
 * =============================================================================
 * Implementation file for TMAG5273 driver (explicit instantiation).
 * This file exists for better IDE navigation and debugging support.
 * =============================================================================
 */

#include "TMAG5273.h"

// The driver is header-only with inline implementations.
// This file ensures the compiler generates code for the template
// and provides a single compilation unit for debugging symbols.

// No additional implementation needed - all methods are inline in header.
// This file serves as a compilation anchor for the driver.
```

- [ ] **Step 3: Create unit test for TMAG5273**

```cpp
// glove_firmware/tests/test_tmag5273.cpp
#include <unity.h>
#include <Wire.h>
#include "lib/Sensors/TCA9548A.h"
#include "lib/Sensors/TMG5273.h"

static TCA9548A* mux = nullptr;
static TMAG5273* sensor = nullptr;

void setUp(void) {
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    mux = new TCA9548A(0x70, &Wire);
    TEST_ASSERT_TRUE(mux->begin());
    sensor = new TMAG5273(mux, 0, 0x22, &Wire);
}

void tearDown(void) {
    delete sensor;
    delete mux;
    sensor = nullptr;
    mux = nullptr;
}

void test_sensor_begin(void) {
    TEST_ASSERT_TRUE(sensor->begin());
}

void test_read_xyz(void) {
    #ifdef HARDWARE_TEST
    float x, y, z;
    bool success = sensor->readXYZ(&x, &y, &z);
    TEST_ASSERT_TRUE(success);
    // Verify values are within ±40mT range
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, z);
    #endif
}

void test_temperature_read(void) {
    #ifdef HARDWARE_TEST
    float temp = sensor->readTemperature();
    TEST_ASSERT_FALSE(isnan(temp));
    // Room temperature should be roughly 15-35°C
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 25.0f, temp);
    #endif
}

void test_mux_channel(void) {
    TEST_ASSERT_EQUAL(0, sensor->getMuxChannel());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_begin);
    RUN_TEST(test_read_xyz);
    RUN_TEST(test_temperature_read);
    RUN_TEST(test_mux_channel);
    UNITY_END();
}
```

- [ ] **Step 4: Run test and verify it passes**

```powershell
cd glove_firmware
pio test -e native -v --filter test_tmag5273
```

Expected: Tests pass (hardware tests require actual sensor)

- [ ] **Step 5: Commit**

```bash
git add glove_firmware/lib/Sensors/TMG5273.cpp glove_firmware/tests/test_tmag5273.cpp
git commit -m "feat(tmag5273): add implementation file and unit tests"
```

---

### Task 1.3: BNO085 IMU Driver (using Adafruit library)

**Files:**
- Modify: `glove_firmware/lib/Sensors/SensorManager.h` (add BNO085 integration)
- Test: `glove_firmware/tests/test_bno085.cpp`

- [ ] **Step 1: Configure Adafruit BNO08x library**

The `platformio.ini` already includes:
```ini
adafruit/Adafruit BNO08x @ ^1.2.3
adafruit/Adafruit BusIO @ ^1.14.3
```

Verify the library supports:
1. I2C communication (not SPI)
2. Game Rotation Vector (report ID 0x05)
3. Gyroscope calibrated (report ID 0x0D)
4. 100Hz output frequency configuration

- [ ] **Step 2: Add BNO085 wrapper to SensorManager.h**

```cpp
// Add to glove_firmware/lib/Sensors/SensorManager.h

#include <Adafruit_BNO08x.h>
#include <Adafruit_BNO08x/Ultra9DOF.h>

class SensorManager {
public:
    // ... existing members ...

    /**
     * @brief Initialize BNO085 IMU.
     * @return true if IMU initialized and sensors enabled.
     */
    bool initBNO085() {
        if (!_bno85.begin_I2C(0x4A, &Wire)) {
            Serial.println("[BNO085] ERROR: Cannot find sensor at 0x4A");
            return false;
        }

        // Enable Game Rotation Vector (no magnetic interference)
        if (!_bno85.enableReport(SH2_GAME_ROTATION_VECTOR, 10)) {  // 10ms = 100Hz
            Serial.println("[BNO085] ERROR: Cannot enable GRV report");
            return false;
        }

        // Enable calibrated gyroscope
        if (!_bno85.enableReport(SH2_GYROSCOPE_CALIBRATED, 10)) {
            Serial.println("[BNO085] ERROR: Cannot enable gyro report");
            return false;
        }

        Serial.println("[BNO085] IMU initialized (GRV + Gyro @ 100Hz)");
        return true;
    }

    /**
     * @brief Read BNO085 quaternion and gyroscope.
     * @param quat_out  Output quaternion [w, x, y, z] (normalized, no magnetic)
     * @param gyro_out  Output gyroscope [gx, gy, gz] in rad/s
     * @return true if new data available.
     */
    bool readBNO085(float* quat_out, float* gyro_out) {
        if (!_bno85.wasReset()) {
            return false;
        }

        // Get Game Rotation Vector
        sensors_event_t event;
        if (_bno85.getGameRotationVector(&event)) {
            quat_out[0] = event.quaternion.w;
            quat_out[1] = event.quaternion.x;
            quat_out[2] = event.quaternion.y;
            quat_out[3] = event.quaternion.z;
        }

        // Get calibrated gyroscope
        if (_bno85.getGyro(&event)) {
            gyro_out[0] = event.gyro.x;
            gyro_out[1] = event.gyro.y;
            gyro_out[2] = event.gyro.z;
        }

        return true;
    }

    /**
     * @brief Convert quaternion to Euler angles (degrees).
     * @param quat  Input quaternion [w, x, y, z]
     * @param euler_out  Output [roll, pitch, yaw] in degrees
     */
    static void quaternionToEuler(const float* quat, float* euler_out) {
        float w = quat[0], x = quat[1], y = quat[2], z = quat[3];

        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        euler_out[0] = atan2f(sinr_cosp, cosr_cosp) * RAD_TO_DEG;

        // Pitch (y-axis rotation)
        float sinp = 2.0f * (w * y - z * x);
        if (fabsf(sinp) >= 1.0f) {
            euler_out[1] = copysignf(M_PI / 2.0f, sinp) * RAD_TO_DEG;  // 90 degrees
        } else {
            euler_out[1] = asinf(sinp) * RAD_TO_DEG;
        }

        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        euler_out[2] = atan2f(siny_cosp, cosy_cosp) * RAD_TO_DEG;
    }

private:
    Adafruit_BNO08x _bno85;
};
```

- [ ] **Step 3: Create BNO085 test**

```cpp
// glove_firmware/tests/test_bno085.cpp
#include <unity.h>
#include "lib/Sensors/SensorManager.h"

static SensorManager* sensor_mgr = nullptr;

void setUp(void) {
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    sensor_mgr = new SensorManager();
}

void tearDown(void) {
    delete sensor_mgr;
    sensor_mgr = nullptr;
}

void test_bno085_init(void) {
    #ifdef HARDWARE_TEST
    bool success = sensor_mgr->initBNO085();
    TEST_ASSERT_TRUE(success);
    #endif
}

void test_bno085_read(void) {
    #ifdef HARDWARE_TEST
    float quat[4], gyro[4];
    bool success = sensor_mgr->readBNO085(quat, gyro);
    TEST_ASSERT_TRUE(success);
    // Quaternion should be normalized (magnitude ≈ 1)
    float mag = sqrtf(quat[0]*quat[0] + quat[1]*quat[1] + 
                       quat[2]*quat[2] + quat[3]*quat[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, mag);
    #endif
}

void test_quat_to_euler(void) {
    // Test identity quaternion (no rotation)
    float identity[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float euler[3];
    SensorManager::quaternionToEuler(identity, euler);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, euler[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, euler[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, euler[2]);

    // Test 90-degree pitch
    float pitch90[4] = {0.707f, 0.0f, 0.707f, 0.0f};
    SensorManager::quaternionToEuler(pitch90, euler);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 90.0f, euler[1]);  // Allow numerical error
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bno085_init);
    RUN_TEST(test_bno085_read);
    RUN_TEST(test_quat_to_euler);
    UNITY_END();
}
```

- [ ] **Step 4: Run test and commit**

```powershell
cd glove_firmware
pio test -e native -v --filter test_bno085

git add glove_firmware/lib/Sensors/SensorManager.h glove_firmware/tests/test_bno085.cpp
git commit -m "feat(bno085): add IMU driver integration and Euler conversion"
```

---

### Task 1.4: SensorManager — Unified Sensor HAL

**Files:**
- Modify: `glove_firmware/lib/Sensors/SensorManager.h`
- Create: `glove_firmware/lib/Sensors/SensorManager.cpp`
- Test: `glove_firmware/tests/test_sensor_manager.cpp`

- [ ] **Step 1: Create complete SensorManager class**

```cpp
// glove_firmware/lib/Sensors/SensorManager.h
/* =============================================================================
 * EdgeAI Data Glove V3 — Unified Sensor Manager
 * =============================================================================
 * Manages all sensors: TCA9548A mux, 5× TMAG5273 Hall sensors, BNO085 IMU.
 * Provides single initialization and read interface for Task_SensorRead.
 * =============================================================================
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "TCA9548A.h"
#include "TMAG5273.h"
#include "data_structures.h"

class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    /**
     * @brief Initialize all sensors and I2C bus.
     * @return true if all critical sensors initialized successfully.
     */
    bool begin();

    /**
     * @brief Read all sensors (blocking, ~5-8ms).
     * @param out  Output structure for sensor data.
     * @return true if read successful.
     */
    bool readAll(SensorData* out);

    /**
     * @brief Calibrate sensors (zero-point reference).
     * Call with glove in known neutral position.
     */
    void calibrate();

    // Static utility (for BNO085 quaternion → Euler conversion)
    static void quaternionToEuler(const float* quat, float* euler_out);

private:
    TCA9548A _mux;
    TMAG5273 _hall_sensors[NUM_HALL_SENSORS];
    Adafruit_BNO08x _bno085;

    bool _initMux();
    bool _initHallSensors();
    bool _initBNO085();
};

#endif // SENSOR_MANAGER_H
```

- [ ] **Step 2: Create SensorManager.cpp implementation**

```cpp
// glove_firmware/lib/Sensors/SensorManager.cpp
/* =============================================================================
 * EdgeAI Data Glove V3 — Sensor Manager Implementation
 * =============================================================================
 */

#include "SensorManager.h"
#include <math.h>

SensorManager::SensorManager()
    : _mux(0x70, &Wire),
      _hall_sensors{
          TMAG5273(&_mux, MuxChannels::HALL_SENSOR_0, 0x22, &Wire),
          TMAG5273(&_mux, MuxChannels::HALL_SENSOR_1, 0x22, &Wire),
          TMAG5273(&_mux, MuxChannels::HALL_SENSOR_2, 0x22, &Wire),
          TMAG5273(&_mux, MuxChannels::HALL_SENSOR_3, 0x22, &Wire),
          TMAG5273(&_mux, MuxChannels::HALL_SENSOR_4, 0x22, &Wire)
      } {}

SensorManager::~SensorManager() {}

bool SensorManager::begin() {
    Serial.println("[SensorManager] Initializing...");

    // Initialize I2C bus
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);

    // Initialize components in order
    if (!_initMux()) {
        Serial.println("[SensorManager] ERROR: Mux init failed");
        return false;
    }

    if (!_initHallSensors()) {
        Serial.println("[SensorManager] ERROR: Hall sensors init failed");
        return false;
    }

    if (!_initBNO085()) {
        Serial.println("[SensorManager] WARNING: BNO085 init failed");
        // Continue without IMU for debugging
    }

    Serial.println("[SensorManager] Initialization complete");
    return true;
}

bool SensorManager::readAll(SensorData* out) {
    if (out == nullptr) {
        return false;
    }

    out->timestamp_us = esp_timer_get_time();

    // Read BNO085 IMU first (on dedicated mux channel)
    float quat[4], gyro[3];
    if (_bno085.wasReset()) {
        sensors_event_t event;
        if (_bno085.getGameRotationVector(&event)) {
            out->quaternion[0] = event.quaternion.w;
            out->quaternion[1] = event.quaternion.x;
            out->quaternion[2] = event.quaternion.y;
            out->quaternion[3] = event.quaternion.z;
        }
        if (_bno085.getGyro(&event)) {
            out->gyro[0] = event.gyro.x;
            out->gyro[1] = event.gyro.y;
            out->gyro[2] = event.gyro.z;
        }
        // Convert to Euler angles
        quaternionToEuler(out->quaternion, out->euler);
    }

    // Read 5 Hall sensors sequentially
    for (uint8_t i = 0; i < NUM_HALL_SENSORS; i++) {
        float x, y, z;
        if (_hall_sensors[i].readXYZ(&x, &y, &z)) {
            out->hall_xyz[i * 3 + 0] = x;
            out->hall_xyz[i * 3 + 1] = y;
            out->hall_xyz[i * 3 + 2] = z;
        }
    }

    return true;
}

void SensorManager::calibrate() {
    Serial.println("[SensorManager] Calibrating (place glove in neutral position)...");
    delay(2000);  // Allow user to position glove

    SensorData ref;
    readAll(&ref);

    // Store reference values (to be used in normalization)
    // TODO: Implement calibration storage in NVS

    Serial.println("[SensorManager] Calibration complete");
}

void SensorManager::quaternionToEuler(const float* quat, float* euler_out) {
    float w = quat[0], x = quat[1], y = quat[2], z = quat[3];

    // Roll
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    euler_out[0] = atan2f(sinr_cosp, cosr_cosp) * RAD_TO_DEG;

    // Pitch
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f) {
        euler_out[1] = copysignf(M_PI / 2.0f, sinp) * RAD_TO_DEG;
    } else {
        euler_out[1] = asinf(sinp) * RAD_TO_DEG;
    }

    // Yaw
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    euler_out[2] = atan2f(siny_cosp, cosy_cosp) * RAD_TO_DEG;
}

bool SensorManager::_initMux() {
    return _mux.begin();
}

bool SensorManager::_initHallSensors() {
    uint8_t detected = 0;
    for (uint8_t i = 0; i < NUM_HALL_SENSORS; i++) {
        if (_hall_sensors[i].begin()) {
            detected++;
        }
    }
    Serial.printf("[SensorManager] %d/%d Hall sensors detected\n", detected, NUM_HALL_SENSORS);
    return detected >= 3;  // Require at least 3 sensors
}

bool SensorManager::_initBNO085() {
    if (!_bno085.begin_I2C(0x4A, &Wire)) {
        return false;
    }

    // Enable reports at 100Hz (10ms interval)
    if (!_bno085.enableReport(SH2_GAME_ROTATION_VECTOR, 10)) {
        return false;
    }
    if (!_bno085.enableReport(SH2_GYROSCOPE_CALIBRATED, 10)) {
        return false;
    }

    Serial.println("[SensorManager] BNO085 IMU enabled");
    return true;
}
```

- [ ] **Step 3: Create SensorManager test**

```cpp
// glove_firmware/tests/test_sensor_manager.cpp
#include <unity.h>
#include "lib/Sensors/SensorManager.h"

static SensorManager* sensor_mgr = nullptr;

void setUp(void) {
    sensor_mgr = new SensorManager();
}

void tearDown(void) {
    delete sensor_mgr;
    sensor_mgr = nullptr;
}

void test_sensor_manager_begin(void) {
    #ifdef HARDWARE_TEST
    bool success = sensor_mgr->begin();
    TEST_ASSERT_TRUE(success);
    #endif
}

void test_sensor_manager_read(void) {
    #ifdef HARDWARE_TEST
    SensorManager mgr;
    mgr.begin();

    SensorData data;
    bool success = mgr.readAll(&data);
    TEST_ASSERT_TRUE(success);

    // Verify timestamp is recent (within 1 second)
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    TEST_ASSERT_UINT32_WITHIN(1000, now, data.timestamp_us / 1000);
}

void test_euler_conversion(void) {
    // Test various quaternions
    float test_cases[][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},   // Identity
        {0.707f, 0.0f, 0.707f, 0.0f}, // 90° pitch
        {0.707f, 0.0f, 0.0f, 0.707f}, // 90° yaw
        {0.707f, 0.707f, 0.0f, 0.0f}, // 90° roll
    };
    float expected[][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 90.0f, 0.0f},
        {0.0f, 0.0f, 90.0f},
        {90.0f, 0.0f, 0.0f},
    };

    for (int i = 0; i < 4; i++) {
        float euler[3];
        SensorManager::quaternionToEuler(test_cases[i], euler);
        TEST_ASSERT_FLOAT_WITHIN(5.0f, expected[i][0], euler[0]);
        TEST_ASSERT_FLOAT_WITHIN(5.0f, expected[i][1], euler[1]);
        TEST_ASSERT_FLOAT_WITHIN(5.0f, expected[i][2], euler[2]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_manager_begin);
    RUN_TEST(test_sensor_manager_read);
    RUN_TEST(test_euler_conversion);
    UNITY_END();
}
```

- [ ] **Step 4: Run test and commit**

```powershell
cd glove_firmware
pio test -e native -v --filter test_sensor_manager

git add glove_firmware/lib/Sensors/SensorManager.h glove_firmware/lib/Sensors/SensorManager.cpp glove_firmware/tests/test_sensor_manager.cpp
git commit -m "feat(sensors): implement unified SensorManager HAL"
```

---

### Task 1.5: Update main.cpp with Task_SensorRead Implementation

**Files:**
- Modify: `glove_firmware/src/main.cpp`

- [ ] **Step 1: Implement Task_SensorRead with full sensor reading**

```cpp
// Replace Task_SensorRead implementation in main.cpp

// Add at top with other includes
#include "lib/Sensors/SensorManager.h"
#include "lib/Filters/KalmanFilter1D.h"

// Add global objects (replace existing placeholders)
static SensorManager g_sensor_manager;
static KalmanFilter1D<float> g_kalman_filters[FEATURE_COUNT];

void Task_SensorRead(void* pvParameters) {
    Serial.println("[Task_SensorRead] Started on Core 1");

    // Initialize sensors
    if (!g_sensor_manager.begin()) {
        Serial.println("[Task_SensorRead] ERROR: Sensor init failed!");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Initialize Kalman filters
    // Q=process noise, R=measurement noise
    for (uint8_t i = 0; i < HALL_FEATURE_COUNT; i++) {
        g_kalman_filters[i].setProcessNoise(1e-4f);
        g_kalman_filters[i].setMeasurementNoise(1e-2f);
    }
    // IMU filters (slightly different tuning)
    for (uint8_t i = HALL_FEATURE_COUNT; i < FEATURE_COUNT; i++) {
        g_kalman_filters[i].setProcessNoise(1e-3f);
        g_kalman_filters[i].setMeasurementNoise(5e-3f);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100 Hz

    uint32_t frame_count = 0;

    while (true) {
        SensorData data;
        data.seq = frame_count;

        // Read all sensors
        if (g_sensor_manager.readAll(&data)) {
            // Apply Kalman filtering to all features
            float features[FEATURE_COUNT];
            data.toFeatureArray(features);

            for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
                features[i] = g_kalman_filters[i].update(features[i]);
            }

            // Push to data queue (non-blocking)
            if (xQueueSend(g_data_queue, &data, 0) != pdPASS) {
                Serial.printf("[Task_SensorRead] Queue full, dropped frame %lu\n", frame_count);
            }

            // Print debug every 100 frames
            if (frame_count % 100 == 0) {
                Serial.printf("[Task_SensorRead] Frame %lu, Free heap: %lu\n",
                             frame_count, ESP.getFreeHeap());
            }
        }

        frame_count++;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

- [ ] **Step 2: Build and verify compilation**

```powershell
cd glove_firmware
pio run -e esp32-s3-devkitc-1-n16r8
```

Expected: Build succeeds with no errors

- [ ] **Step 3: Commit**

```bash
git add glove_firmware/src/main.cpp
git commit -m "feat(main): implement Task_SensorRead with Kalman filtering"
```

---

## Phase 2: 信号处理与数据采集

### Task 2.1: Sliding Window Ring Buffer

**Files:**
- Create: `glove_firmware/lib/Filters/SlidingWindow.h`
- Create: `glove_firmware/lib/Filters/SlidingWindow.cpp`
- Test: `glove_firmware/tests/test_sliding_window.cpp`

- [ ] **Step 1: Create SlidingWindow class**

```cpp
// glove_firmware/lib/Filters/SlidingWindow.h
/* =============================================================================
 * EdgeAI Data Glove V3 — Sliding Window Ring Buffer
 * =============================================================================
 * Circular buffer for 30-frame (300ms @ 100Hz) sensor data windows.
 * Used as input buffer for L1 inference (TFLite Micro).
 *
 * Thread-safe: Designed for single producer (Task_SensorRead) and
 * single consumer (Task_Inference).
 * =============================================================================
 */

#ifndef SLIDING_WINDOW_H
#define SLIDING_WINDOW_H

#include "data_structures.h"
#include <atomic>

class SlidingWindow {
public:
    SlidingWindow();
    ~SlidingWindow();

    /**
     * @brief Add a new frame to the window (oldest is overwritten).
     * @param features  Array of FEATURE_COUNT floats.
     * @return Index where frame was stored (0 to WINDOW_SIZE-1).
     */
    uint16_t push(const float* features);

    /**
     * @brief Get pointer to flat buffer (for inference).
     * @return Pointer to contiguous memory: WINDOW_SIZE × FEATURE_COUNT floats.
     */
    const float* getBuffer() const;

    /**
     * @brief Check if window is full (ready for inference).
     * @return true if at least WINDOW_SIZE frames have been pushed.
     */
    bool isFull() const;

    /**
     * @brief Get number of frames currently in buffer.
     * @return Frame count (1 to WINDOW_SIZE).
     */
    uint16_t getFrameCount() const;

    /**
     * @brief Reset buffer to empty state.
     */
    void clear();

private:
    float* _buffer;                          // Contiguous memory: [frame0, frame1, ...]
    uint16_t _write_idx;                     // Next write position
    std::atomic<uint16_t> _frame_count;      // Total frames pushed (saturates)
    bool _full;                              // Has wrapped around at least once
};

#endif // SLIDING_WINDOW_H
```

- [ ] **Step 2: Create SlidingWindow.cpp implementation**

```cpp
// glove_firmware/lib/Filters/SlidingWindow.cpp
/* =============================================================================
 * EdgeAI Data Glove V3 — Sliding Window Implementation
 * =============================================================================
 */

#include "SlidingWindow.h"
#include <string.h>

SlidingWindow::SlidingWindow()
    : _frame_count(0), _full(false) {

    // Allocate contiguous buffer in PSRAM
    size_t total_floats = WINDOW_SIZE * FEATURE_COUNT;
    _buffer = (float*)heap_caps_malloc(total_floats * sizeof(float), MALLOC_CAP_SPIRAM);

    if (_buffer == nullptr) {
        Serial.println("[SlidingWindow] ERROR: PSRAM allocation failed!");
        // Fallback to internal heap (will be slower)
        _buffer = (float*)malloc(total_floats * sizeof(float));
    }

    _write_idx = 0;
    memset(_buffer, 0, total_floats * sizeof(float));

    Serial.printf("[SlidingWindow] Allocated %d bytes (%d frames × %d features)\n",
                  total_floats * sizeof(float), WINDOW_SIZE, FEATURE_COUNT);
}

SlidingWindow::~SlidingWindow() {
    free(_buffer);
}

uint16_t SlidingWindow::push(const float* features) {
    // Calculate write position in flat buffer
    uint16_t frame_pos = _write_idx % WINDOW_SIZE;
    float* frame_ptr = _buffer + (frame_pos * FEATURE_COUNT);

    // Copy features
    memcpy(frame_ptr, features, FEATURE_COUNT * sizeof(float));

    // Update state
    _write_idx++;
    if (_frame_count < WINDOW_SIZE) {
        _frame_count++;
    }
    if (_write_idx >= WINDOW_SIZE) {
        _full = true;
    }

    return frame_pos;
}

const float* SlidingWindow::getBuffer() const {
    return _buffer;
}

bool SlidingWindow::isFull() const {
    return _full;
}

uint16_t SlidingWindow::getFrameCount() const {
    return _frame_count.load();
}

void SlidingWindow::clear() {
    _write_idx = 0;
    _frame_count = 0;
    _full = false;
    memset(_buffer, 0, WINDOW_SIZE * FEATURE_COUNT * sizeof(float));
}
```

- [ ] **Step 3: Create SlidingWindow test**

```cpp
// glove_firmware/tests/test_sliding_window.cpp
#include <unity.h>
#include <string.h>
#include "lib/Filters/SlidingWindow.h"

static SlidingWindow* window = nullptr;

void setUp(void) {
    window = new SlidingWindow();
}

void tearDown(void) {
    delete window;
    window = nullptr;
}

void test_initial_state(void) {
    TEST_ASSERT_FALSE(window->isFull());
    TEST_ASSERT_EQUAL(0, window->getFrameCount());
}

void test_push_single_frame(void) {
    float features[FEATURE_COUNT];
    memset(features, 0x1, sizeof(features));

    uint16_t pos = window->push(features);
    TEST_ASSERT_EQUAL(0, pos);
    TEST_ASSERT_EQUAL(1, window->getFrameCount());
    TEST_ASSERT_FALSE(window->isFull());
}

void test_fill_window(void) {
    float features[FEATURE_COUNT];
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        memset(features, i, sizeof(features));
        window->push(features);
    }

    TEST_ASSERT_TRUE(window->isFull());
    TEST_ASSERT_EQUAL(WINDOW_SIZE, window->getFrameCount());
}

void test_overwrite_oldest(void) {
    // Fill window
    float features[FEATURE_COUNT];
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        memset(features, i, sizeof(features));
        window->push(features);
    }

    // Push one more (should overwrite frame 0)
    memset(features, 0xFF, sizeof(features));
    window->push(features);

    TEST_ASSERT_TRUE(window->isFull());
    TEST_ASSERT_EQUAL(WINDOW_SIZE, window->getFrameCount());
}

void test_clear(void) {
    // Fill and clear
    float features[FEATURE_COUNT];
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        window->push(features);
    }

    window->clear();

    TEST_ASSERT_FALSE(window->isFull());
    TEST_ASSERT_EQUAL(0, window->getFrameCount());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_push_single_frame);
    RUN_TEST(test_fill_window);
    RUN_TEST(test_overwrite_oldest);
    RUN_TEST(test_clear);
    UNITY_END();
}
```

- [ ] **Step 4: Run test and commit**

```powershell
cd glove_firmware
pio test -e native -v --filter test_sliding_window

git add glove_firmware/lib/Filters/SlidingWindow.h glove_firmware/lib/Filters/SlidingWindow.cpp glove_firmware/tests/test_sliding_window.cpp
git commit -m "feat(filters): implement sliding window ring buffer"
```

---

### Task 2.2: Feature Normalization

**Files:**
- Create: `glove_firmware/lib/Filters/FeatureNormalizer.h`
- Create: `glove_firmware/lib/Filters/FeatureNormalizer.cpp`
- Test: `glove_firmware/tests/test_normalizer.cpp`

- [ ] **Step 1: Create FeatureNormalizer class**

```cpp
// glove_firmware/lib/Filters/FeatureNormalizer.h
/* =============================================================================
 * EdgeAI Data Glove V3 — Feature Normalization
 * =============================================================================
 * Min-Max normalization for sensor features.
 * Each feature channel has independent min/max bounds.
 *
 * Calibration mode: Collect min/max statistics during calibration phase.
 * Inference mode: Apply stored min/max for normalization.
 * =============================================================================
 */

#ifndef FEATURE_NORMALIZER_H
#define FEATURE_NORMALIZER_H

#include "data_structures.h"

class FeatureNormalizer {
public:
    FeatureNormalizer();
    ~FeatureNormalizer();

    /**
     * @brief Reset statistics for calibration.
     */
    void reset();

    /**
     * @brief Update min/max statistics with a new sample.
     * @param features  Array of FEATURE_COUNT floats.
     */
    void updateStats(const float* features);

    /**
     * @brief Normalize features using stored min/max.
     * @param features  Input/output array of FEATURE_COUNT floats.
     * @param epsilon   Small value to prevent division by zero.
     */
    void normalize(float* features, float epsilon = 1e-6f);

    /**
     * @brief Check if statistics have been collected.
     * @return true if at least one sample has been seen.
     */
    bool isCalibrated() const;

    /**
     * @brief Get feature range (max - min) for a channel.
     * @param channel  Feature index (0 to FEATURE_COUNT-1).
     * @return Range value, or 0 if not calibrated.
     */
    float getRange(uint8_t channel) const;

private:
    float _min[FEATURE_COUNT];
    float _max[FEATURE_COUNT];
    bool _calibrated;
};

#endif // FEATURE_NORMALIZER_H
```

- [ ] **Step 2: Create FeatureNormalizer.cpp implementation**

```cpp
// glove_firmware/lib/Filters/FeatureNormalizer.cpp
/* =============================================================================
 * EdgeAI Data Glove V3 — Feature Normalization Implementation
 * =============================================================================
 */

#include "FeatureNormalizer.h"
#include <string.h>
#include <algorithm>

FeatureNormalizer::FeatureNormalizer() : _calibrated(false) {
    reset();
}

FeatureNormalizer::~FeatureNormalizer() {}

void FeatureNormalizer::reset() {
    // Initialize with extreme values
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        _min[i] = FLT_MAX;
        _max[i] = FLT_MIN;
    }
    _calibrated = false;
}

void FeatureNormalizer::updateStats(const float* features) {
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        float val = features[i];
        if (val < _min[i]) {
            _min[i] = val;
        }
        if (val > _max[i]) {
            _max[i] = val;
        }
    }
    _calibrated = true;
}

void FeatureNormalizer::normalize(float* features, float epsilon) {
    if (!_calibrated) {
        return;  // Cannot normalize without statistics
    }

    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        float range = _max[i] - _min[i];
        if (range < epsilon) {
            // Feature has no variance, center at 0.5
            features[i] = 0.5f;
        } else {
            // Min-Max normalize to [0, 1]
            features[i] = (features[i] - _min[i]) / range;
            // Clamp to [0, 1] for safety
            features[i] = std::max(0.0f, std::min(1.0f, features[i]));
        }
    }
}

bool FeatureNormalizer::isCalibrated() const {
    return _calibrated;
}

float FeatureNormalizer::getRange(uint8_t channel) const {
    if (channel >= FEATURE_COUNT || !_calibrated) {
        return 0.0f;
    }
    return _max[channel] - _min[channel];
}
```

- [ ] **Step 3: Create normalizer test**

```cpp
// glove_firmware/tests/test_normalizer.cpp
#include <unity.h>
#include <math.h>
#include "lib/Filters/FeatureNormalizer.h"

static FeatureNormalizer* normalizer = nullptr;

void setUp(void) {
    normalizer = new FeatureNormalizer();
}

void tearDown(void) {
    delete normalizer;
    normalizer = nullptr;
}

void test_initial_state(void) {
    TEST_ASSERT_FALSE(normalizer->isCalibrated());
}

void test_update_stats(void) {
    float features[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        features[i] = (float)i;
    }

    normalizer->updateStats(features);
    TEST_ASSERT_TRUE(normalizer->isCalibrated());

    // Channel 5 should have min=5, max=5
    TEST_ASSERT_EQUAL_FLOAT(0.0f, normalizer->getRange(5));
}

void test_normalize_single_sample(void) {
    // First sample: min = max, so normalized = 0.5
    float features[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        features[i] = 10.0f;
    }

    normalizer->updateStats(features);
    normalizer->normalize(features);

    // All features should be 0.5 (no variance)
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, features[i]);
    }
}

void test_normalize_range(void) {
    // Two samples to establish range
    float min_sample[FEATURE_COUNT];
    float max_sample[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        min_sample[i] = 0.0f;
        max_sample[i] = 100.0f;
    }

    normalizer->updateStats(min_sample);
    normalizer->updateStats(max_sample);

    // Test normalization of midpoint
    float mid[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        mid[i] = 50.0f;
    }
    normalizer->normalize(mid);

    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, mid[i]);
    }
}

void test_normalize_clamping(void) {
    // Establish range [0, 100]
    float min_sample[FEATURE_COUNT], max_sample[FEATURE_COUNT];
    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
        min_sample[i] = 0.0f;
        max_sample[i] = 100.0f;
    }
    normalizer->updateStats(min_sample);
    normalizer->updateStats(max_sample);

    // Out-of-range values should be clamped
    float out_of_range[FEATURE_COUNT];
    out_of_range[0] = -50.0f;   // Below min
    out_of_range[1] = 150.0f;   // Above max

    normalizer->normalize(out_of_range);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out_of_range[0]);  // Clamped to 0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, out_of_range[1]);  // Clamped to 1
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_update_stats);
    RUN_TEST(test_normalize_single_sample);
    RUN_TEST(test_normalize_range);
    RUN_TEST(test_normalize_clamping);
    UNITY_END();
}
```

- [ ] **Step 4: Run test and commit**

```powershell
cd glove_firmware
pio test -e native -v --filter test_normalizer

git add glove_firmware/lib/Filters/FeatureNormalizer.h glove_firmware/lib/Filters/FeatureNormalizer.cpp glove_firmware/tests/test_normalizer.cpp
git commit -m "feat(filters): implement feature normalization"
```

---

### Task 2.3: Integrate Signal Processing Pipeline in main.cpp

**Files:**
- Modify: `glove_firmware/src/main.cpp`

- [ ] **Step 1: Add SlidingWindow and FeatureNormalizer to main.cpp**

```cpp
// Add includes at top of main.cpp
#include "lib/Filters/SlidingWindow.h"
#include "lib/Filters/FeatureNormalizer.h"

// Add global objects
static SlidingWindow g_sliding_window;
static FeatureNormalizer g_normalizer;

// Update setup() to initialize normalizer
void setup() {
    // ... existing code ...

    // Initialize normalizer in calibration mode
    g_normalizer.reset();
    Serial.println("[setup] Feature normalizer initialized");

    // ... rest of existing code ...
}
```

- [ ] **Step 2: Update Task_SensorRead to use sliding window**

```cpp
// Update Task_SensorRead in main.cpp

void Task_SensorRead(void* pvParameters) {
    Serial.println("[Task_SensorRead] Started on Core 1");

    // Initialize sensors
    if (!g_sensor_manager.begin()) {
        Serial.println("[Task_SensorRead] ERROR: Sensor init failed!");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Initialize Kalman filters
    for (uint8_t i = 0; i < HALL_FEATURE_COUNT; i++) {
        g_kalman_filters[i].setProcessNoise(1e-4f);
        g_kalman_filters[i].setMeasurementNoise(1e-2f);
    }
    for (uint8_t i = HALL_FEATURE_COUNT; i < FEATURE_COUNT; i++) {
        g_kalman_filters[i].setProcessNoise(1e-3f);
        g_kalman_filters[i].setMeasurementNoise(5e-3f);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100 Hz
    uint32_t frame_count = 0;
    uint32_t calibration_frames = 0;
    const uint32_t CALIBRATION_DURATION = 200;  // 2 seconds = 200 frames @ 100Hz

    Serial.println("[Task_SensorRead] Calibration mode: 2 seconds...");

    while (true) {
        SensorData data;
        data.seq = frame_count;

        if (g_sensor_manager.readAll(&data)) {
            // Extract features
            float features[FEATURE_COUNT];
            data.toFeatureArray(features);

            // Apply Kalman filtering
            for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
                features[i] = g_kalman_filters[i].update(features[i]);
            }

            // Calibration phase: collect statistics (first 2 seconds)
            if (calibration_frames < CALIBRATION_DURATION) {
                g_normalizer.updateStats(features);
                calibration_frames++;

                if (calibration_frames == CALIBRATION_DURATION) {
                    Serial.printf("[Task_SensorRead] Calibration complete. Feature ranges:\n");
                    for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
                        Serial.printf("  [%d] range = %.4f\n", i, g_normalizer.getRange(i));
                    }
                }
            }

            // Normalize features
            g_normalizer.normalize(features);

            // Push to sliding window
            g_sliding_window.push(features);

            // When window is full, wake inference task
            if (g_sliding_window.isFull()) {
                // Signal inference task (to be implemented)
                // xTaskNotify(TaskInferenceHandle, 1, eSetBits);
            }

            // Push to data queue for Comms
            if (xQueueSend(g_data_queue, &data, 0) != pdPASS) {
                Serial.printf("[Task_SensorRead] Queue full, dropped frame %lu\n", frame_count);
            }

            // Debug output every 100 frames
            if (frame_count % 100 == 0) {
                Serial.printf("[Task_SensorRead] Frame %lu, Window: %d/%d\n",
                             frame_count, g_sliding_window.getFrameCount(), WINDOW_SIZE);
            }
        }

        frame_count++;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

- [ ] **Step 3: Build and verify**

```powershell
cd glove_firmware
pio run -e esp32-s3-devkitc-1-n16r8
```

Expected: Build succeeds

- [ ] **Step 4: Commit**

```bash
git add glove_firmware/src/main.cpp
git commit -m "feat(main): integrate full signal processing pipeline"
```

---

## Summary

This plan covers:

**Phase 1 (HAL):**
- TCA9548A I2C multiplexer driver (verified)
- TMAG5273 Hall sensor driver (verified)
- BNO085 IMU integration
- SensorManager unified HAL
- FreeRTOS task framework with Task_SensorRead

**Phase 2 (Signal Processing):**
- Kalman filter (1D, 21 channels)
- Sliding window ring buffer (30 frames)
- Feature normalization (Min-Max)
- Full pipeline integration

**Total: ~10 files created/modified, ~8 test files, ~10 commits**

After completing this plan, the firmware will:
1. Read all sensors at 100Hz
2. Apply Kalman filtering to all 21 features
3. Normalize features using collected statistics
4. Maintain a 30-frame sliding window for inference
5. Output debug data via serial

The next phase (Phase 3) will add L1 TFLite Micro inference.
