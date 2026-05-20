/* =============================================================================
 * EdgeAI Data Glove V3 — Sensor Manager
 * =============================================================================
 * Unified interface for all sensors on the glove:
 *   - 5× TMAG5273 3D Hall sensors (via TCA9548A I2C mux)
 *   - 1× BNO085 9-DOF IMU (quaternion, euler, gyro)
 *
 * Responsibilities:
 *   1. Initialize I2C bus (SDA=GPIO8, SCL=GPIO9, 400 kHz)
 *   2. Initialize TCA9548A mux and scan for all downstream sensors
 *   3. Read all sensors into a unified SensorData struct
 *   4. Apply Kalman filtering to all 21 signal channels
 *   5. Fall back to simulation mode when hardware sensors unavailable
 *
 * V3.1: Added SIMULATION MODE — when TCA9548A is not detected, generates
 * synthetic sensor data for 20 distinct gesture classes. This enables the
 * full signal processing pipeline (Kalman → Normalizer → SlidingWindow →
 * CSV output) to operate without physical sensors, allowing Edge Impulse
 * data collection and model training to proceed in parallel with hardware
 * assembly.
 *
 * Thread Safety:
 *   This class is called from Task_SensorRead on Core 1. No other task
 *   should call readAll() — use the FreeRTOS dataQueue to pass data.
 * =============================================================================
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>

#include "data_structures.h"
#include "TCA9548A.h"
#include "TMG5273.h"
#include "../Filters/KalmanFilter1D.h"

// BNO085 library headers
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO08x.h>

class SensorManager {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    SensorManager()
        : _mux(TCA9548A::DEFAULT_ADDR, &Wire),
          _hall{
              TMAG5273(&_mux, MuxChannels::HALL_SENSOR_0, TMAG5273::DEFAULT_ADDR, &Wire),
              TMAG5273(&_mux, MuxChannels::HALL_SENSOR_1, TMAG5273::DEFAULT_ADDR, &Wire),
              TMAG5273(&_mux, MuxChannels::HALL_SENSOR_2, TMAG5273::DEFAULT_ADDR, &Wire),
              TMAG5273(&_mux, MuxChannels::HALL_SENSOR_3, TMAG5273::DEFAULT_ADDR, &Wire),
              TMAG5273(&_mux, MuxChannels::HALL_SENSOR_4, TMAG5273::DEFAULT_ADDR, &Wire)
          },
          _imu(),
          _initialized(false), _imu_ok(false), _seq(0),
          _simulation_mode(false), _sim_gesture_id(0),
          _sim_frame_counter(0), _sim_transition_timer(0) {
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize I2C bus, mux, all Hall sensors, and BNO085 IMU.
     * Falls back to simulation mode if TCA9548A is not detected.
     * @return true if initialized (real or simulated).
     */
    bool begin() {
        Serial.println("========================================");
        Serial.println("[SensorManager] V3 Initialization Start");
        Serial.println("========================================");

        // ---- Step 1: Initialize I2C ----
        Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
        Wire.setTimeOut(50);
        Serial.printf("[SensorManager] I2C initialized: SDA=%d, SCL=%d, %lu kHz\n",
                      I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ / 1000);
        delay(10);

        // ---- Step 2: Initialize TCA9548A mux ----
        if (!_mux.begin()) {
            Serial.println("[SensorManager] WARNING: TCA9548A not found!");
            Serial.println("[SensorManager] Entering SIMULATION mode");
            Serial.println("[SensorManager] 20 gesture classes, 3s cycle each");
            _simulation_mode = true;
            _initialized = true;
            _imu_ok = false;
            Serial.println("========================================");
            return true;
        }

        // ---- Step 3: Scan mux channels and initialize Hall sensors ----
        uint8_t hall_ok = 0;
        for (uint8_t i = 0; i < NUM_HALL_SENSORS; i++) {
            if (_hall[i].begin()) {
                hall_ok++;
            }
        }
        Serial.printf("[SensorManager] Hall sensors: %d/%d initialized\n",
                      hall_ok, NUM_HALL_SENSORS);

        // ---- Step 4: Initialize BNO085 IMU ----
        if (!initIMU()) {
            Serial.println("[SensorManager] WARNING: BNO085 IMU not available!");
        }

        _initialized = (hall_ok > 0);
        Serial.printf("[SensorManager] Init %s\n",
                      _initialized ? "SUCCESS" : "FAILED");
        Serial.println("========================================");
        return _initialized;
    }

    // =========================================================================
    // Sensor Reading
    // =========================================================================

    /**
     * @brief Read all sensors and fill SensorData struct.
     * In simulation mode, generates synthetic gesture data.
     * @param data  Reference to SensorData to fill.
     * @return true if data was populated.
     */
    bool readAll(SensorData& data) {
        if (!_initialized) return false;

        data.zero();
        data.timestamp_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
        data.seq = _seq++;

        if (_simulation_mode) {
            return readSimulated(data);
        }

        bool any_success = false;

        // ---- Read Hall Sensors ----
        for (uint8_t i = 0; i < NUM_HALL_SENSORS; i++) {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (_hall[i].readXYZ(&x, &y, &z)) {
                uint8_t idx = i * 3;
                data.hall_xyz[idx + 0] = _kf_hall[idx + 0].update(x);
                data.hall_xyz[idx + 1] = _kf_hall[idx + 1].update(y);
                data.hall_xyz[idx + 2] = _kf_hall[idx + 2].update(z);
                any_success = true;
            } else {
                uint8_t idx = i * 3;
                data.hall_xyz[idx + 0] = _kf_hall[idx + 0].getEstimate();
                data.hall_xyz[idx + 1] = _kf_hall[idx + 1].getEstimate();
                data.hall_xyz[idx + 2] = _kf_hall[idx + 2].getEstimate();
            }
        }

        // ---- Read BNO085 IMU ----
        if (_imu_ok) {
            readIMU(data);
        }

        return any_success;
    }

    void resetFilters() {
        for (auto& kf : _kf_hall) kf.reset();
        for (auto& kf : _kf_imu) kf.reset();
        Serial.println("[SensorManager] All Kalman filters reset");
    }

    // =========================================================================
    // Status
    // =========================================================================

    bool isInitialized() const { return _initialized; }
    bool isIMUAvailable() const { return _imu_ok; }
    bool isSimulation() const { return _simulation_mode; }

    /// Switch simulated gesture (0-19). Only meaningful in simulation mode.
    void setSimGesture(uint8_t gesture_id) {
        if (gesture_id < 20) {
            _sim_gesture_id = gesture_id;
            _sim_transition_timer = millis();
        }
    }

private:
    // =========================================================================
    // Members
    // =========================================================================

    TCA9548A  _mux;
    TMAG5273  _hall[NUM_HALL_SENSORS];
    Adafruit_BNO08x _imu;
    bool      _initialized;
    bool      _imu_ok;
    uint32_t  _seq;

    KalmanFilter1D<float> _kf_hall[HALL_FEATURE_COUNT];
    KalmanFilter1D<float> _kf_imu[IMU_FEATURE_COUNT];

    bool      _simulation_mode;
    uint8_t   _sim_gesture_id;
    uint32_t  _sim_frame_counter;
    uint32_t  _sim_transition_timer;

    sh2_SensorValue_t _sensor_value;
    bool _quat_report_received;

    // =========================================================================
    // BNO085 IMU Initialization
    // =========================================================================

    bool initIMU() {
        _imu_ok = false;
        _quat_report_received = false;

        _mux.selectChannel(MuxChannels::BNO085_IMU);

        if (!_imu.begin_I2C(0x4A, &Wire)) {
            Serial.println("[SensorManager] BNO085 begin() failed");
            _mux.disableAll();
            return false;
        }

        if (!_imu.enableReport(SH2_GAME_ROTATION_VECTOR, 10000)) {
            Serial.println("[SensorManager] BNO085: failed to enable game rotation vector");
            _mux.disableAll();
            return false;
        }

        if (!_imu.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000)) {
            Serial.println("[SensorManager] BNO085: failed to enable gyroscope");
            _mux.disableAll();
            return false;
        }

        uint32_t t0 = millis();
        while (!_quat_report_received && (millis() - t0) < 500) {
            _imu.getSensorEvent(&_sensor_value);
            if (_sensor_value.sensorId == SH2_GAME_ROTATION_VECTOR) {
                _quat_report_received = true;
            }
            delay(1);
        }

        _mux.disableAll();
        _imu_ok = _quat_report_received;

        Serial.printf("[SensorManager] BNO085 IMU: %s\n",
                      _imu_ok ? "OK" : "FAILED (no data)");
        return _imu_ok;
    }

    // =========================================================================
    // BNO085 IMU Reading
    // =========================================================================

    void readIMU(SensorData& data) {
        _mux.selectChannel(MuxChannels::BNO085_IMU);

        for (int i = 0; i < 10; i++) {
            if (!_imu.getSensorEvent(&_sensor_value)) break;

            switch (_sensor_value.sensorId) {
                case SH2_GAME_ROTATION_VECTOR: {
                    float q_w = _sensor_value.un.gameRotationVector.real;
                    float q_x = _sensor_value.un.gameRotationVector.i;
                    float q_y = _sensor_value.un.gameRotationVector.j;
                    float q_z = _sensor_value.un.gameRotationVector.k;

                    data.quaternion[0] = _kf_imu[0].update(q_w);
                    data.quaternion[1] = _kf_imu[1].update(q_x);
                    data.quaternion[2] = _kf_imu[2].update(q_y);
                    data.quaternion[3] = _kf_imu[3].update(q_z);

                    quatToEuler(q_w, q_x, q_y, q_z,
                                data.euler[0], data.euler[1], data.euler[2]);
                    break;
                }

                case SH2_GYROSCOPE_CALIBRATED:
                    data.gyro[0] = _kf_imu[4].update(
                        _sensor_value.un.gyroscope.x * 180.0f / PI);
                    data.gyro[1] = _kf_imu[5].update(
                        _sensor_value.un.gyroscope.y * 180.0f / PI);
                    data.gyro[2] = _kf_imu[6].update(
                        _sensor_value.un.gyroscope.z * 180.0f / PI);
                    break;
            }
        }

        _mux.disableAll();
    }

    // =========================================================================
    // Quaternion → Euler Conversion
    // =========================================================================

    static void quatToEuler(float qw, float qx, float qy, float qz,
                            float& roll, float& pitch, float& yaw) {
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / PI;

        float sinp = 2.0f * (qw * qy - qz * qx);
        if (fabsf(sinp) >= 1.0f)
            pitch = copysignf(90.0f, sinp);
        else
            pitch = asinf(sinp) * 180.0f / PI;

        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / PI;
    }

    // =========================================================================
    // Simulation Mode
    // =========================================================================

    bool readSimulated(SensorData& data) {
        _sim_frame_counter++;

        struct GestureSignature {
            float hall[15];
            float roll, pitch, yaw, gx, gy, gz;
        };

        static const GestureSignature gestures[20] = {
            // 0: open hand
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 1: fist
            {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 2: thumb up
            {{0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, -45,0,0, 0,0,0},
            // 3: OK sign
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 4: peace
            {{0,0,0.9f, 0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 5: index point
            {{0,0,0.9f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 6: middle
            {{0,0,0.9f, 0,0,0.9f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 7: pinky up
            {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 8: L shape
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 9: three fingers
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
            // 10: open + roll +30
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 30,0,0, 0,0,0},
            // 11: fist + roll -30
            {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, -30,0,0, 0,0,0},
            // 12: open + pitch +45
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,45,0, 0,0,0},
            // 13: fist + pitch -45
            {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,-45,0, 0,0,0},
            // 14: half curl
            {{0,0,0.5f, 0,0,0.5f, 0,0,0.5f, 0,0,0.5f, 0,0,0.5f}, 0,0,0, 0,0,0},
            // 15: thumb only curled
            {{0,0,0.9f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 16: index only curled
            {{0,0,0.1f, 0,0,0.9f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 17: middle only curled
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 18: ring only curled
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.9f, 0,0,0.1f}, 0,0,0, 0,0,0},
            // 19: pinky only curled
            {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.9f}, 0,0,0, 0,0,0},
        };

        GestureSignature g = gestures[_sim_gesture_id];

        float noise = 0.05f;
        for (uint8_t i = 0; i < HALL_FEATURE_COUNT; i++) {
            float n = (esp_random() % 1001 - 500) / 500.0f * noise;
            data.hall_xyz[i] = _kf_hall[i].update(g.hall[i] + n);
        }

        data.euler[0] = _kf_imu[0].update(g.roll + (esp_random() % 1001 - 500) / 500.0f * 2.0f);
        data.euler[1] = _kf_imu[1].update(g.pitch + (esp_random() % 1001 - 500) / 500.0f * 2.0f);
        data.euler[2] = _kf_imu[2].update(g.yaw + (esp_random() % 1001 - 500) / 500.0f * 2.0f);
        data.quaternion[0] = 1.0f;

        data.gyro[0] = g.gx + (esp_random() % 1001 - 500) / 500.0f * 0.5f;
        data.gyro[1] = g.gy + (esp_random() % 1001 - 500) / 500.0f * 0.5f;
        data.gyro[2] = g.gz + (esp_random() % 1001 - 500) / 500.0f * 0.5f;

        if (_sim_frame_counter % 300 == 0) {
            uint8_t next = (_sim_gesture_id + 1) % 20;
            Serial.printf("[Sim] gesture %d → %d (frame %lu)\n",
                          _sim_gesture_id, next, (unsigned long)_sim_frame_counter);
            _sim_gesture_id = next;
        }

        return true;
    }
};

#endif // SENSOR_MANAGER_H