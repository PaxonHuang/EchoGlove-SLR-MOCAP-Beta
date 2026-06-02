/* =============================================================================
 * EchoGlove V5 — Sensor Manager (Simulation Mode)
 * =============================================================================
 * Pure-simulation SensorManager for V5 firmware.
 *
 * Hardware sensors (TMAG5273, TCA9548A) have been removed. This module
 * generates synthetic 11-dimensional data:
 *   flex[5] + euler[3] + gyro[3]  =  SINGLE_HAND_FEATURES (11)
 *
 * 20 gesture signatures are built-in. Each gesture has a distinct flex
 * pattern plus characteristic euler/gyro values. Use setSimulatedGesture()
 * to pin a specific gesture, or let it auto-cycle.
 * =============================================================================
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "data_structures.h"

#ifndef UNIT_TEST
#include <Arduino.h>
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

    // =========================================================================
    // Gesture Signature
    // =========================================================================
    //
    // Each gesture defines:
    //   flex[5]  — per-finger flex values [0=open, 1=curled]
    //              finger order: thumb, index, middle, ring, pinky
    //   roll, pitch, yaw  — characteristic wrist orientation (degrees)
    //   gx, gy, gz        — characteristic gyro values (deg/s)

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

    /**
     * @brief Initialize the sensor manager.
     * @param simulation  If true, forces simulation mode (no I2C/hardware).
     * @return true if initialized.
     */
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
        Serial.printf("[SensorManager] %d gesture signatures loaded\n",
                      NUM_GESTURES);
        Serial.println("========================================");
#endif

        return true;
    }

    // =========================================================================
    // Sensor Reading
    // =========================================================================

    /**
     * @brief Read all sensors and return a filled SensorData struct.
     * In simulation mode, generates synthetic gesture data.
     * @return Filled SensorData with incrementing seq and valid values.
     */
    SensorData read() {
        SensorData data;
        data.zero();

        data.seq = _seq++;
        data.timestamp_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);

        if (_simulation_mode) {
            generateSimulated(data);
        }

        return data;
    }

    // =========================================================================
    // Gesture Control
    // =========================================================================

    /**
     * @brief Pin the simulation to a specific gesture (0-19).
     */
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

    // Gesture name lookup (for debug/logging)
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

    // =========================================================================
    // Gesture Table (local static in generateSimulated — ODR-safe, C++14 OK)
    // =========================================================================

    static const GestureSignature* gestures() {
        static const GestureSignature table[NUM_GESTURES] = {
            //  0: open_hand     — all fingers extended
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            //  1: fist           — all fingers fully curled
            {{0.90f, 0.90f, 0.90f, 0.90f, 0.90f},   0,  0,  0,  0, 0, 0},
            //  2: thumbs_up      — thumb extended, rest curled
            {{0.10f, 0.85f, 0.85f, 0.85f, 0.85f}, -45,  0,  0,  0, 0, 0},
            //  3: peace          — index+middle extended, rest curled
            {{0.80f, 0.10f, 0.10f, 0.80f, 0.80f},   0,  0,  0,  0, 0, 0},
            //  4: point          — index extended, rest curled
            {{0.85f, 0.10f, 0.85f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            //  5: ok_sign        — thumb+index touch, rest extended
            {{0.50f, 0.50f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            //  6: three_fingers  — thumb+index+middle extended
            {{0.10f, 0.10f, 0.10f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            //  7: pinky_up       — pinky extended, rest curled
            {{0.85f, 0.85f, 0.85f, 0.85f, 0.10f},   0,  0,  0,  0, 0, 0},
            //  8: l_shape        — thumb+index form L, rest curled
            {{0.10f, 0.10f, 0.85f, 0.85f, 0.85f},   0,  0,  0,  0, 0, 0},
            //  9: grab           — mid-curl all fingers
            {{0.65f, 0.65f, 0.65f, 0.65f, 0.65f},   0,  0,  0,  0, 0, 0},
            // 10: pinch          — thumb+index pinch, others extended
            {{0.45f, 0.45f, 0.05f, 0.05f, 0.05f},   0,  0,  0,  0, 0, 0},
            // 11: wave           — open hand, wrist tilted
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},  30,  0,  0,  5, 0, 0},
            // 12: flat_hand      — all fingers straight, palm down
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0, 90,  0,  0, 0, 0},
            // 13: claw           — fingers half-curled, aggressive
            {{0.55f, 0.55f, 0.55f, 0.55f, 0.55f},   0, 20,  0,  0, 0, 0},
            // 14: hook           — index+middle hooked, rest curled
            {{0.80f, 0.70f, 0.70f, 0.10f, 0.10f},   0,  0,  0,  0, 0, 0},
            // 15: fingers_spread — all fingers spread wide
            {{0.05f, 0.05f, 0.05f, 0.05f, 0.05f},   0,  0, 30,  0, 0, 0},
            // 16: two_fingers_up — index+middle up (vertical)
            {{0.85f, 0.10f, 0.10f, 0.85f, 0.85f},   0,  0,  0,  0, 5, 0},
            // 17: fist_thumb_out — fist with thumb sticking out
            {{0.10f, 0.90f, 0.90f, 0.90f, 0.90f},   0,  0,  0,  0, 0, 0},
            // 18: half_curl      — all fingers at 50%
            {{0.50f, 0.50f, 0.50f, 0.50f, 0.50f},   0,  0,  0,  0, 0, 0},
            // 19: thumbs_down    — thumb curled down, rest extended
            {{0.90f, 0.05f, 0.05f, 0.05f, 0.05f},  45,  0,  0,  0, 0, 0},
        };
        return table;
    }

    // =========================================================================
    // Simulation Data Generator
    // =========================================================================

    void generateSimulated(SensorData& data) {
        _sim_frame_counter++;

        const GestureSignature& g = gestures()[_sim_gesture_id];

        // Add small sensor noise for realism
        const float flex_noise = 0.02f;
        const float euler_noise = 1.5f;
        const float gyro_noise = 0.3f;

        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            float n = randomNoise(flex_noise);
            data.flex[i] = clampf(g.flex[i] + n, 0.0f, 1.0f);
        }

        // euler: roll, pitch, yaw stored consecutively in struct
        data.euler[0] = g.roll + randomNoise(euler_noise);
        data.euler[1] = g.pitch + randomNoise(euler_noise);
        data.euler[2] = g.yaw + randomNoise(euler_noise);

        data.gyro[0] = g.gx + randomNoise(gyro_noise);
        data.gyro[1] = g.gy + randomNoise(gyro_noise);
        data.gyro[2] = g.gz + randomNoise(gyro_noise);

        // Quaternion: identity (w=1, x=y=z=0)
        data.quaternion[0] = 1.0f;
        data.quaternion[1] = 0.0f;
        data.quaternion[2] = 0.0f;
        data.quaternion[3] = 0.0f;
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
