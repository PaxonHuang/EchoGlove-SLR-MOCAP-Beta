/* =============================================================================
 * EchoGlove V5 — Sensor Manager
 * =============================================================================
 * Unified sensor manager for V5 firmware.
 *
 * Hardware mode (simulation=false):
 *   - Flex sensors: 5x Spectra Symbol via 2x ADS1115 (0x48, 0x49)
 *   - IMU: BNO085 (0x4B) via Adafruit BNO08x library
 *   - I2C bus: flat, GPIO8=SDA, GPIO9=SCL, 100kHz
 *
 * Simulation mode (simulation=true):
 *   - Generates synthetic 11-dimensional data with 20 gesture signatures
 *   - No I2C hardware access
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
#include <Adafruit_BNO08x.h>
#include "ADS1115Manager.h"
#include "FlexManager.h"
#else
// Stubs for native test builds
static uint32_t _sim_millis = 0;
static inline uint32_t millis() { return _sim_millis += 10; }
static inline uint32_t esp_timer_get_time() { return (uint32_t)millis() * 1000; }
static inline uint32_t esp_random() { return (uint32_t)rand(); }
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
          _sim_frame_counter(0) {
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
        Serial.printf("[SensorManager] V5 init: mode=%s\n",
                      _simulation_mode ? "SIMULATION" : "HARDWARE");

        if (!_simulation_mode) {
            // ── Initialize I2C bus first ──
            Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
            Serial.printf("[SensorManager] I2C bus init: SDA=%d SCL=%d %dkHz\n",
                          I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ / 1000);

            // ── I2C Bus Scan (verify devices before init) ──
            Serial.println("[SensorManager] Scanning I2C bus...");
            int found = 0;
            for (uint8_t addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                uint8_t err = Wire.endTransmission();
                if (err == 0) {
                    Serial.printf("  ✓ Found device at 0x%02X", addr);
                    if (addr == 0x48) Serial.print("  (ADS1115 #1)");
                    else if (addr == 0x49) Serial.print("  (ADS1115 #2)");
                    else if (addr == 0x4B) Serial.print("  (BNO085)");
                    Serial.println();
                    found++;
                }
            }
            Serial.printf("[SensorManager] I2C scan: %d device(s) found\n", found);

            // ── Initialize ADS1115 (flex sensors) ──
            // Note: ADS1115Manager.begin() will call Wire.begin() again, but that's OK
            bool adc_ok = _adc.begin(false);
            Serial.printf("[SensorManager] ADS1115: %s\n", adc_ok ? "OK" : "FAIL");

            // ── Initialize FlexManager with ADS1115 ──
            _flex.begin(&_adc);
            Serial.println("[SensorManager] FlexManager: linked to ADS1115");

            // ── BNO085 IMU: SKIP for now — debug ADS1115 first ──
            _bno_ok = false;
            Serial.println("[SensorManager] BNO085: SKIPPED (debugging ADS1115)");
            // TODO: Re-enable after ADS1115 is verified
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
    // Hardware drivers
    ADS1115Manager  _adc;
    FlexManager     _flex;
    Adafruit_BNO08x _bno;
    bool            _bno_ok = false;
#endif

    // =========================================================================
    // Hardware Reading
    // =========================================================================

    void readHardware(SensorData& data) {
#ifndef UNIT_TEST
        // ── Flex sensors (ADS1115) ──
        _flex.read(data.flex);

        // ── IMU (BNO085) ──
        if (_bno_ok) {
            sh2_SensorValue_t sensor;
            float qw = 1, qx = 0, qy = 0, qz = 0;
            bool got_quat = false;

            // Read all available reports
            while (_bno.getSensorEvent(&sensor)) {
                if (sensor.sensorId == SH2_ROTATION_VECTOR) {
                    qw = sensor.un.rotationVector.real;
                    qx = sensor.un.rotationVector.i;
                    qy = sensor.un.rotationVector.j;
                    qz = sensor.un.rotationVector.k;
                    got_quat = true;
                }
                if (sensor.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                    data.gyro[0] = sensor.un.gyroscope.x * 57.2958f; // rad/s → deg/s
                    data.gyro[1] = sensor.un.gyroscope.y * 57.2958f;
                    data.gyro[2] = sensor.un.gyroscope.z * 57.2958f;
                }
            }

            // Store quaternion
            data.quaternion[0] = qw;
            data.quaternion[1] = qx;
            data.quaternion[2] = qy;
            data.quaternion[3] = qz;

            // Quaternion → Euler (ZYX convention)
            if (got_quat) {
                data.euler[0] = atan2f(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy)) * 57.2958f;
                float sinp = 2*(qw*qy - qz*qx);
                data.euler[1] = (fabsf(sinp) >= 1) ? copysignf(90.0f, sinp) * 57.2958f : asinf(sinp) * 57.2958f;
                data.euler[2] = atan2f(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz)) * 57.2958f;
            }
        }
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
