/* =============================================================================
 * EchoGlove V6 — Flex Sensor Manager with Calibration
 * =============================================================================
 * V6: Refactored to depend on IFlexSensor* (Strategy/Adapter), not a
 * concrete ADC. The flex source (InternalADCManager or ADS1115FlexAdapter)
 * is injected at begin(); this class no longer #includes any concrete ADC.
 *
 * Calibration (600 frames at 100 Hz = 6 seconds):
 *   Phase 1 (300 frames): record raw min values from open hand
 *   Phase 2 (300 frames): record raw max values from fist
 *   Normalized: output[i] = (raw[i] - min[i]) / (max[i] - min[i]), clamped [0,1]
 *   On completion: persists raw min/max via IFlexSensor::persistCalibration()
 *                 (D-ADC-3 — fixes V5 RAM-only bug)
 *
 * V5 compatibility: begin(bool simulation) is preserved for legacy tests;
 *   simulation mode produces a deterministic 0.0 output.
 * =============================================================================
 */

#pragma once
#include "data_structures.h"
#include "IFlexSensor.h"

class FlexManager {
public:
    FlexManager()
        : _sensor(nullptr),
          _simulation(false),
          _calibrated(false),
          _calibrating(false),
          _cal_frame_count(0) {
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            _cal_min[i] = 65535;
            _cal_max[i] = 0;
            _sim_values[i] = 0.0f;
        }
    }

    // ---- V6: inject any IFlexSensor implementation ----
    bool begin(IFlexSensor* sensor) {
        _sensor = sensor;
        _simulation = false;
        _calibrated = false;
        _calibrating = false;
        _cal_frame_count = 0;
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            _cal_min[i] = 65535;
            _cal_max[i] = 0;
        }
        return (_sensor != nullptr);
    }

    // ---- V5 compatibility: simulation mode (no sensor) ----
    bool begin(bool simulation = false) {
        _sensor = nullptr;
        _simulation = simulation;
        _calibrated = false;
        _calibrating = false;
        _cal_frame_count = 0;
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            _cal_min[i] = 65535;
            _cal_max[i] = 0;
        }
        return true;
    }

    bool read(float* values) {
        uint16_t raw[NUM_FLEX_SENSORS] = {0};

        if (_simulation || !_sensor) {
            // Simulation / no-sensor fallback: zeros (legacy contract)
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) raw[i] = 0;
        } else {
            if (!_sensor->readRaw(raw)) {
                if (values) {
                    for (int i = 0; i < NUM_FLEX_SENSORS; i++) values[i] = 0.0f;
                }
                return false;
            }
        }

        // ---- Calibration tracking (raw uint16_t bounds) ----
        if (_calibrating) {
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                if (raw[i] < _cal_min[i]) _cal_min[i] = raw[i];
                if (raw[i] > _cal_max[i]) _cal_max[i] = raw[i];
            }
            _cal_frame_count++;
            if (_cal_frame_count >= CALIBRATION_FRAMES) {
                _calibrating = false;
                _calibrated = true;
                // Persist raw min/max to NVS via the interface (D-ADC-3)
                if (_sensor) _sensor->persistCalibration();
            }
        }

        // ---- Normalization to [0, 1] ----
        if (values) {
            if (_calibrated) {
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                    uint16_t range = _cal_max[i] - _cal_min[i];
                    if (range < 1) range = 1;
                    float n = static_cast<float>(raw[i] - _cal_min[i]) /
                              static_cast<float>(range);
                    if (n < 0.0f) n = 0.0f;
                    if (n > 1.0f) n = 1.0f;
                    values[i] = n;
                }
            } else {
                // No calibration: default 12-bit range normalization
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                    values[i] = static_cast<float>(raw[i]) / 4095.0f;
                    if (values[i] > 1.0f) values[i] = 1.0f;
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

    // V5 compat: simulation value injection (legacy tests)
    void setSimulatedValues(const float* values) {
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) _sim_values[i] = values[i];
    }

private:
    static constexpr int CALIBRATION_FRAMES = 600;  // 6s @ 100 Hz

    IFlexSensor* _sensor;
    bool         _simulation;
    bool         _calibrated;
    bool         _calibrating;
    int          _cal_frame_count;
    uint16_t     _cal_min[NUM_FLEX_SENSORS];
    uint16_t     _cal_max[NUM_FLEX_SENSORS];
    float        _sim_values[NUM_FLEX_SENSORS];
};
