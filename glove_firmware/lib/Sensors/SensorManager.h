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
          _initialized(false), _imu_ok(false), _seq(0) {
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize I2C bus, mux, all Hall sensors, and BNO085 IMU.
     * @return true if all sensors initialized successfully.
     */
    bool begin() {
        Serial.println("========================================");
        Serial.println("[SensorManager] V3 Initialization Start");
        Serial.println("========================================");

        // ---- Step 1: Initialize I2C ----
        Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
        Wire.setTimeOut(50);  // 50 ms I2C timeout
        Serial.printf("[SensorManager] I2C initialized: SDA=%d, SCL=%d, %lu kHz\n",
                      I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ / 1000);
        delay(10);

        // ---- Step 2: Initialize TCA9548A mux ----
        if (!_mux.begin()) {
            Serial.println("[SensorManager] FATAL: TCA9548A not found!");
            return false;
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
        if (hall_ok < NUM_HALL_SENSORS) {
            Serial.println("[SensorManager] WARNING: Some Hall sensors missing!");
        }

        // ---- Step 4: Initialize BNO085 IMU ----
        if (!initIMU()) {
            Serial.println("[SensorManager] WARNING: BNO085 IMU not available!");
            // Non-fatal — Hall sensors alone can still operate
        }

        _initialized = (hall_ok > 0);  // At least one Hall sensor required
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
     *
     * Reads all 5 Hall sensors (with mux channel switching) and the BNO085 IMU.
     * Applies Kalman filtering to all 21 raw signals before writing to output.
     *
     * Expected call rate: 100 Hz from Task_SensorRead.
     * Typical execution time: 6–12 ms (dominated by Hall conversions).
     *
     * @param data  Reference to SensorData to fill.
     * @return true if at least one sensor read succeeded.
     */
    bool readAll(SensorData& data) {
        if (!_initialized) return false;

        data.zero();
        data.timestamp_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
        data.seq = _seq++;

        bool any_success = false;

        // ---- Read Hall Sensors (mux-switched, sequential) ----
        for (uint8_t i = 0; i < NUM_HALL_SENSORS; i++) {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (_hall[i].readXYZ(&x, &y, &z)) {
                // Apply Kalman filter to each axis
                uint8_t idx = i * 3;
                data.hall_xyz[idx + 0] = _kf_hall[idx + 0].update(x);
                data.hall_xyz[idx + 1] = _kf_hall[idx + 1].update(y);
                data.hall_xyz[idx + 2] = _kf_hall[idx + 2].update(z);
                any_success = true;
            } else {
                // Sensor read failed — keep previous filtered value (or 0)
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

    /**
     * @brief Reset all Kalman filters (useful after calibration or model switch).
     */
    void resetFilters() {
        for (auto& kf : _kf_hall) {
            kf.reset();
        }
        for (auto& kf : _kf_imu) {
            kf.reset();
        }
        Serial.println("[SensorManager] All Kalman filters reset");
    }

    // =========================================================================
    // Status
    // =========================================================================

    bool isInitialized() const { return _initialized; }
    bool isIMUAvailable() const { return _imu_ok; }

private:
    // =========================================================================
    // Members
    // =========================================================================

    TCA9548A  _mux;                               ///< I2C multiplexer
    TMAG5273  _hall[NUM_HALL_SENSORS];             ///< Hall sensor array
    Adafruit_BNO08x _imu;                         ///< BNO085 IMU
    bool      _initialized;                        ///< Overall init status
    bool      _imu_ok;                             ///< IMU init status
    uint32_t  _seq;                                ///< Sequence counter

    /// Kalman filters for all 21 signals (15 Hall + 6 IMU)
    KalmanFilter1D<float> _kf_hall[HALL_FEATURE_COUNT];
    KalmanFilter1D<float> _kf_imu[IMU_FEATURE_COUNT];

    // BNO085 sensor report IDs
    sh2_SensorValue_t _sensor_value;
    bool _quat_report_received;

    // =========================================================================
    // BNO085 IMU Initialization
    // =========================================================================

    /**
     * @brief Configure the BNO085 to report rotation vector (quaternion)
     *        and gyroscope data at 100 Hz.
     * @return true if IMU is responsive.
     */
    bool initIMU() {
        _imu_ok = false;
        _quat_report_received = false;

        // Select the IMU's mux channel
        _mux.selectChannel(MuxChannels::BNO085_IMU);

        if (!_imu.begin_I2C(0x4A, &Wire)) {
            Serial.println("[SensorManager] BNO085 begin() failed");
            _mux.disableAll();
            return false;
        }

        // Enable Game Rotation Vector report (quaternion, no geomagnetic)
        // at 100 Hz — SOP spec §4.2 requires this over SH2_ROTATION_VECTOR
        // to avoid magnetic interference in wrist-mounted IMU
        if (!_imu.enableReport(SH2_GAME_ROTATION_VECTOR, 10000)) {  // 10 ms = 100 Hz
            Serial.println("[SensorManager] BNO085: failed to enable game rotation vector");
            _mux.disableAll();
            return false;
        }

        // Enable gyroscope report at 100 Hz
        if (!_imu.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000)) {
            Serial.println("[SensorManager] BNO085: failed to enable gyroscope");
            _mux.disableAll();
            return false;
        }

        // Wait for first report to confirm data flow
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

    /**
     * @brief Read IMU data and fill quaternion, euler, gyro in SensorData.
     *
     * Reads up to 10 sensor events looking for rotation vector and gyro.
     * Converts quaternion to euler angles. Applies Kalman filtering.
     */
    void readIMU(SensorData& data) {
        // Select IMU mux channel
        _mux.selectChannel(MuxChannels::BNO085_IMU);

        // Drain up to 10 events from the BNO085 FIFO
        for (int i = 0; i < 10; i++) {
            if (!_imu.getSensorEvent(&_sensor_value)) {
                break;
            }

            switch (_sensor_value.sensorId) {
                case SH2_GAME_ROTATION_VECTOR: {
                    // Quaternion: w, x, y, z (normalized by BNO085, no magnetic)
                    float q_w = _sensor_value.un.gameRotationVector.real;
                    float q_x = _sensor_value.un.gameRotationVector.i;
                    float q_y = _sensor_value.un.gameRotationVector.j;
                    float q_z = _sensor_value.un.gameRotationVector.k;

                    // Store quaternion (Kalman filtered — though quaternion
                    // is already fused internally by BNO085 SH-2 sensor hub)
                    data.quaternion[0] = _kf_imu[0].update(q_w);
                    data.quaternion[1] = _kf_imu[1].update(q_x);
                    data.quaternion[2] = _kf_imu[2].update(q_y);
                    data.quaternion[3] = _kf_imu[3].update(q_z);

                    // Convert quaternion to Euler angles (degrees)
                    quatToEuler(q_w, q_x, q_y, q_z,
                                data.euler[0], data.euler[1], data.euler[2]);
                    break;
                }

                case SH2_GYROSCOPE_CALIBRATED: {
                    // Gyroscope: x, y, z in rad/s → convert to deg/s
                    data.gyro[0] = _kf_imu[4].update(
                        _sensor_value.un.gyroscope.x * 180.0f / PI);
                    data.gyro[1] = _kf_imu[5].update(
                        _sensor_value.un.gyroscope.y * 180.0f / PI);
                    data.gyro[2] = _kf_imu[6].update(
                        _sensor_value.un.gyroscope.z * 180.0f / PI);
                    break;
                }

                default:
                    break;
            }
        }

        // Release mux channel
        _mux.disableAll();
    }

    // =========================================================================
    // Quaternion → Euler Conversion
    // =========================================================================

    /**
     * @brief Convert quaternion to Euler angles (roll, pitch, yaw) in degrees.
     *
     * Convention: ZYX intrinsic rotation (Tait-Bryan angles).
     *   Roll  (X) = atan2(2(qw*qx + qy*qz), 1 - 2(qx² + qy²))
     *   Pitch (Y) = asin(2(qw*qy - qz*qx))
     *   Yaw   (Z) = atan2(2(qw*qz + qx*qy), 1 - 2(qy² + qz²))
     */
    static void quatToEuler(float qw, float qx, float qy, float qz,
                            float& roll, float& pitch, float& yaw) {
        // Roll (X-axis rotation)
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / PI;

        // Pitch (Y-axis rotation)
        float sinp = 2.0f * (qw * qy - qz * qx);
        if (fabsf(sinp) >= 1.0f) {
            pitch = copysignf(90.0f, sinp);  // Use 90° if out of range
        } else {
            pitch = asinf(sinp) * 180.0f / PI;
        }

        // Yaw (Z-axis rotation)
        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / PI;
    }
};

#endif // SENSOR_MANAGER_H
