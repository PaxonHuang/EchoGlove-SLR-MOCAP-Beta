/* =============================================================================
 * EchoGlove V5 — Flex Sensor Manager with Calibration
 * =============================================================================
 * Manages 5 flex sensors via ADS1115 ADC with calibration support.
 *
 * Calibration (600 frames at 100 Hz = 6 seconds):
 *   Phase 1 (300 frames): record min values from open hand
 *   Phase 2 (300 frames): record max values from fist
 *   Normalized: output[i] = (raw[i] - min[i]) / (max[i] - min[i]), clamped [0,1]
 *
 * Simulation: setSimulatedValues() feeds deterministic data for tests.
 * =============================================================================
 */

#pragma once
#include "data_structures.h"
#include "ADS1115Manager.h"

class FlexManager {
public:
    bool begin(bool simulation = false) {
        simulation_ = simulation;
        adc_ = nullptr;
        calibrated_ = false;
        calibrating_ = false;
        return true;
    }

    bool begin(ADS1115Manager* adc) {
        simulation_ = false;
        adc_ = adc;
        calibrated_ = false;
        calibrating_ = false;
        return true;
    }

    bool read(float* values) {
        float raw[NUM_FLEX_SENSORS];
        if (simulation_) {
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) raw[i] = sim_values_[i];
        } else if (adc_) {
            adc_->readAll(raw);
        } else {
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) raw[i] = 0;
        }

        if (calibrating_) {
            // Track min/max during calibration
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                if (raw[i] < cal_min_[i]) cal_min_[i] = raw[i];
                if (raw[i] > cal_max_[i]) cal_max_[i] = raw[i];
            }
            cal_frame_count_++;
            if (cal_frame_count_ >= 600) {
                calibrating_ = false;
                calibrated_ = true;
            }
        }

        if (values) {
            if (calibrated_) {
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                    float range = cal_max_[i] - cal_min_[i];
                    if (range < 0.001f) range = 1.0f;
                    values[i] = (raw[i] - cal_min_[i]) / range;
                    if (values[i] < 0.0f) values[i] = 0.0f;
                    if (values[i] > 1.0f) values[i] = 1.0f;
                }
            } else {
                for (int i = 0; i < NUM_FLEX_SENSORS; i++) values[i] = raw[i];
            }
        }
        return true;
    }

    void startCalibration() {
        calibrating_ = true;
        cal_frame_count_ = 0;
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            cal_min_[i] = 1.0f;
            cal_max_[i] = 0.0f;
        }
    }

    bool isCalibrating() const { return calibrating_; }
    bool isCalibrated() const { return calibrated_; }

    void setSimulatedValues(const float* values) {
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) sim_values_[i] = values[i];
    }

private:
    bool simulation_ = false;
    bool calibrated_ = false;
    bool calibrating_ = false;
    int cal_frame_count_ = 0;
    float sim_values_[NUM_FLEX_SENSORS] = {0};
    float cal_min_[NUM_FLEX_SENSORS] = {0};
    float cal_max_[NUM_FLEX_SENSORS] = {0};
    ADS1115Manager* adc_ = nullptr;
};
