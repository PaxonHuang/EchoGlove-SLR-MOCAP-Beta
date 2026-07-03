/* =============================================================================
 * EchoGlove V6 — Sensor Manager
 * =============================================================================
 * Unified sensor manager for V6 firmware.
 *
 * V6 changes (vs V5):
 *   - Flex source: ESP32-S3 internal ADC1 (GPIO1-5) via InternalADCManager
 *     (replaces 2× ADS1115). Injected as IFlexSensor* into FlexManager.
 *   - IMU path: BNO085 still SKIP'd (LSM6DSV16X migration is a separate
 *     V6 task — see project_v6_migration_plan). IMU returns zeros for now.
 *   - I2C bus: only LSM6DSV16X@0x6A will remain once IMU migration lands;
 *     for now no I2C devices are required for the flex-only path.
 *
 * Hardware mode (simulation=false):
 *   - Flex: 5× internal ADC1 (GPIO1-5), N=16 oversample + Kalman + NVS calib
 *   - IMU: zeros (pending LSM6DSV16XManager)
 *
 * Simulation mode (simulation=true):
 *   - Generates synthetic 11-dimensional data with 20 gesture signatures
 *   - No hardware access
 *
 * Output: 11-dim feature vector per hand:
 *   flex[5] + euler[3] + gyro[3]  =  SINGLE_HAND_FEATURES (11)
 * =============================================================================
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "data_structures.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#include <Wire.h>
#include "IFlexSensor.h"
#include "InternalADCManager.h"
#include "FlexManager.h"
#else
// Stubs for native test builds
static uint32_t _sim_millis = 0;
static inline uint32_t millis() { return _sim_millis += 10; }
static inline uint32_t esp_timer_get_time() { return (uint32_t)millis() * 1000; }
static inline uint32_t esp_random() { return (uint32_t)rand(); }
#include "IFlexSensor.h"
#include "InternalADCManager.h"
#include "FlexManager.h"
#endif

class SensorManager {
public:
    static constexpr int NUM_GESTURES = 20;

    struct GestureSignature {
        float flex[NUM_FLEX_SENSORS];
        float roll, pitch, yaw;
        float gx, gy, gz;
    };

    // =========================================================================
    // Construction
    // =========================================================================

    SensorManager()
        : _initialized(false),
          _simulation_mode(false),
          _seq(0),
          _sim_gesture_id(0),
          _sim_frame_counter(0),
          _flex_source(nullptr) {
    }

    ~SensorManager() {
#ifndef UNIT_TEST
        if (_flex_source) {
            delete _flex_source;
            _flex_source = nullptr;
        }
#endif
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    bool begin(bool simulation = true) {
        _simulation_mode = simulation;
        _initialized = true;
        _seq = 1;
        _sim_frame_counter = 0;
        _sim_gesture_id = 0;

#ifndef UNIT_TEST
        Serial.println("========================================");
        Serial.printf("[SensorManager] V6 init: mode=%s\n",
                      _simulation_mode ? "SIMULATION" : "HARDWARE");

        if (!_simulation_mode) {
            // ── V6: Initialize internal ADC1 flex source ──
            _flex_source = new InternalADCManager();
            if (!_flex_source || !_flex_source->begin()) {
                Serial.println("[SensorManager] FATAL: InternalADC init failed");
                return false;
            }
            Serial.println("[SensorManager] InternalADC1: OK (GPIO1-5, N=16)");

            // ── FlexManager linked to IFlexSensor* ──
            _flex.begin(_flex_source);
            Serial.println("[SensorManager] FlexManager: linked to InternalADCManager");

            // ── IMU: SKIP (LSM6DSV16X migration is a separate V6 task) ──
            Serial.println("[SensorManager] IMU: zeros (LSM6DSV16X pending)");
        }

        Serial.printf("[SensorManager] %d gesture signatures loaded\n", NUM_GESTURES);
        Serial.println("========================================");
#endif
        return true;
    }

    // =========================================================================
    // Sensor Reading
    // =========================================================================

    SensorData read() {
        SensorData data;
        data.zero();

        data.seq = _seq++;
        data.timestamp_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);

        if (_simulation_mode) {
            generateSimulated(data);
        } else {
            readHardware(data);
        }

        return data;
    }

    // =========================================================================
    // Gesture Control (simulation only)
    // =========================================================================

    void setSimulatedGesture(int gesture_id) {
        if (gesture_id >= 0 && gesture_id < NUM_GESTURES) {
            _sim_gesture_id = (uint8_t)gesture_id;
        }
    }

    // =========================================================================
    // Status
    // =========================================================================

    bool isInitialized() const { return _initialized; }
    bool isSimulation() const { return _simulation_mode; }
    uint8_t currentGesture() const { return _sim_gesture_id; }

    static const char* gestureName(uint8_t id) {
        static const char* names[NUM_GESTURES] = {
            "open_hand",        "fist",             "thumbs_up",
            "peace",            "point",            "ok_sign",
            "three_fingers",    "pinky_up",         "l_shape",
            "grab",             "pinch",            "wave",
            "flat_hand",        "claw",             "hook",
            "fingers_spread",   "two_fingers_up",   "fist_thumb_out",
            "half_curl",        "thumbs_down"
        };
        return (id < NUM_GESTURES) ? names[id] : "unknown";
    }

private:
    // =========================================================================
    // Members
    // =========================================================================

    bool      _initialized;
    bool      _simulation_mode;
    uint32_t  _seq;

    uint8_t   _sim_gesture_id;
    uint32_t  _sim_frame_counter;

#ifndef UNIT_TEST
    IFlexSensor*   _flex_source;
    FlexManager    _flex;
#else
    IFlexSensor*   _flex_source;
    FlexManager    _flex;
#endif

    // =========================================================================
    // Hardware Reading
    // =========================================================================

    void readHardware(SensorData& data) {
#ifndef UNIT_TEST
        // ── Flex sensors (V6 internal ADC1) ──
        _flex.read(data.flex);

        // ── IMU: zeros until LSM6DSV16X migration lands ──
        // (BNO085 path removed; LSM6DSV16XManager will populate these)
        data.quaternion[0] = 1.0f;
        data.quaternion[1] = 0.0f;
        data.quaternion[2] = 0.0f;
        data.quaternion[3] = 0.0f;
        data.euler[0] = 0.0f;
        data.euler[1] = 0.0f;
        data.euler[2] = 0.0f;
        data.gyro[0] = 0.0f;
        data.gyro[1] = 0.0f;
        data.gyro[2] = 0.0f;
#else
        (void)data;
#endif
    }

    // =========================================================================
    // Simulation Data Generator
    // =========================================================================

    void generateSimulated(SensorData& data) {
        _sim_frame_counter++;

        const GestureSignature& g = gestures()[_sim_gesture_id];

        const float flex_noise = 0.02f;
        const float euler_noise = 1.5f;
        const float gyro_noise = 0.3f;

        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            float n = randomNoise(flex_noise);
            data.flex[i] = clampf(g.flex[i] + n, 0.0f, 1.0f);
        }

        data.euler[0] = g.roll + randomNoise(euler_noise);
        data.euler[1] = g.pitch + randomNoise(euler_noise);
        data.euler[2] = g.yaw + randomNoise(euler_noise);

        data.gyro[0] = g.gx + randomNoise(gyro_noise);
        data.gyro[1] = g.gy + randomNoise(gyro_noise);
        data.gyro[2] = g.gz + randomNoise(gyro_noise);

        data.quaternion[0] = 1.0f;
        data.quaternion[1] = 0.0f;
        data.quaternion[2] = 0.0f;
        data.quaternion[3] = 0.0f;
    }

    // =========================================================================
    // Gesture Table
    // =========================================================================

    static const GestureSignature* gestures() {
        static const GestureSignature table[NUM_GESTURES] = {
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            {{0.90f, 0.90f, 0.90f, 0.90f, 0.90f},   0,  0,  0,  0, 0, 0},
            {{0.10f, 0.85f, 0.85f, 0.85f, 0.85f}, -45,  0,  0,  0, 0, 0},
            {{0.80f, 0.10f, 0.10f, 0.80f, 0.80f},   0,  0,  0,  0, 0, 0},
            {{0.85f, 0.10f, 0.85f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            {{0.50f, 0.50f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            {{0.10f, 0.10f, 0.10f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            {{0.85f, 0.85f, 0.85f, 0.85f, 0.10f},   0,  0,  0,  0, 0, 0},
            {{0.10f, 0.10f, 0.85f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            {{0.65f, 0.65f, 0.65f, 0.65f, 0.65f},   0,  0,  0,  0, 0, 0},
            {{0.45f, 0.45f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},  30,  0,  0,  5, 0, 0},
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0, 90,  0,  0, 0, 0},
            {{0.55f, 0.55f, 0.55f, 0.55f, 0.55f},   0, 20,  0,  0, 0, 0},
            {{0.80f, 0.70f, 0.70f, 0.10f, 0.10f},   0,  0,  0,  0, 0, 0},
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0,  0, 30,  0, 0, 0},
            {{0.85f, 0.10f, 0.10f, 0.85f, 0.85f},   0,  0,  0,  0, 5, 0},
            {{0.10f, 0.90f, 0.90f, 0.90f, 0.90f},   0,  0,  0,  0, 0, 0},
            {{0.50f, 0.50f, 0.50f, 0.50f, 0.50f},   0,  0,  0,  0, 0, 0},
            {{0.90f, 0.05f, 0.05f, 0.05f, 0.05f},  45,  0,  0,  0, 0, 0},
        };
        return table;
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    static float randomNoise(float amplitude) {
        return ((float)(esp_random() % 1001) - 500.0f) / 500.0f * amplitude;
    }

    static float clampf(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }
};

#endif // SENSOR_MANAGER_H
