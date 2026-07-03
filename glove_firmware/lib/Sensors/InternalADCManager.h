/* =============================================================================
 * EchoGlove V6 — InternalADCManager (ESP32-S3 internal ADC1)
 * =============================================================================
 * V6 default IFlexSensor implementation. Replaces 2× ADS1115.
 *
 * Hardware:
 *   - ADC1 channels CH0..CH4 → GPIO1..GPIO5 (Thumb..Pinky)
 *   - Attenuation: ADC_ATTEN_DB_12 (~0-2500 mV usable, up to ~3100 mV)
 *   - Flex divider: 0.90 V (straight) .. 2.15 V (bent), sits inside DB_12 range
 *
 * Sampling:
 *   - N=16 software oversampling per channel → +2 effective bits (~12-13 ENOB)
 *   - Per-channel KalmanFilter1D (wires V5 dead code, D-ADC-6)
 *   - Full 5-ch read < 1 ms (well under 100 Hz / 10 ms budget)
 *
 * Calibration (D-ADC-3, fixes V5 RAM-only bug):
 *   - raw min/max per channel persisted to NVS namespace "flex_cal"
 *   - Keys: "L_min"/"L_max" (blob of 5×uint16), "ver" (UChar schema version)
 *   - Loaded at begin(); FlexManager normalizes via (raw-min)/(max-min)
 *
 * Spec: docs/V6/07_internal_adc_migration.md §5, §6
 * =============================================================================
 */

#pragma once
#include "IFlexSensor.h"
#include <cstdint>

#ifndef UNIT_TEST
#include <Arduino.h>
#include <Preferences.h>
#include "Filters/KalmanFilter1D.h"
#endif

class InternalADCManager : public IFlexSensor {
public:
    // ADC1_CH0..4 → GPIO1..5 (07 spec §3.1)
    static constexpr uint8_t  kPins[5]      = {1, 2, 3, 4, 5};
    static constexpr uint8_t  kOversample   = 16;        // 07 spec §2.3 default
    static constexpr uint16_t kRawMinDef    = 0;
    static constexpr uint16_t kRawMaxDef    = 4095;      // 12-bit
    static constexpr uint8_t  kCalibVersion = 1;         // NVS schema version

    InternalADCManager() : _initialized(false), _has_calib(false) {
        for (int i = 0; i < 5; i++) {
            _min[i] = kRawMinDef;
            _max[i] = kRawMaxDef;
        }
    }

    ~InternalADCManager() override {
#ifndef UNIT_TEST
        for (int i = 0; i < 5; i++) {
            if (_kalman[i]) {
                delete _kalman[i];
                _kalman[i] = nullptr;
            }
        }
#endif
    }

    bool begin() override {
#ifndef UNIT_TEST
        // Configure ADC1 pins: 12 dB attenuation (~0-2.5 V linear region)
        for (int i = 0; i < 5; i++) {
            analogSetPinAttenuation(kPins[i], ADC_ATTEN_DB_12);
            _kalman[i] = new KalmanFilter1D<float>(/*Q*/ 2.0f, /*R*/ 8.0f);
            if (!_kalman[i]) {
                Serial.println("[InternalADC] FATAL: Kalman alloc failed");
                return false;
            }
        }

        // Load calibration from NVS (read-only open)
        _prefs.begin("flex_cal", true);
        _loadCalibration();
        _prefs.end();

        Serial.printf("[InternalADC] init OK: oversample=%u, calib=%s\n",
                      kOversample, _has_calib ? "loaded" : "defaults");
#endif
        _initialized = true;
        return true;
    }

    bool readRaw(uint16_t out[5]) override {
        if (!_initialized) return false;

#ifdef UNIT_TEST
        // Simulation mode for native tests: deterministic mid-range values
        for (int i = 0; i < 5; i++) {
            out[i] = _sim_raw[i];
        }
        return true;
#else
        // Hardware mode: N-sample oversampling + Kalman smoothing
        uint32_t acc[5] = {0};

        for (uint8_t s = 0; s < kOversample; s++) {
            for (uint8_t i = 0; i < 5; i++) {
                acc[i] += analogRead(kPins[i]);
            }
        }

        // Decimate (÷16 via >>4) then Kalman-filter each channel
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t decimated = static_cast<uint16_t>(acc[i] >> 4);
            float filtered = _kalman[i]->update(static_cast<float>(decimated));
            out[i] = static_cast<uint16_t>(filtered + 0.5f);
        }
        return true;
#endif
    }

    bool readMilliVolts(uint16_t out[5]) override {
        if (!_initialized) return false;

#ifdef UNIT_TEST
        for (int i = 0; i < 5; i++) {
            out[i] = _sim_mv[i];
        }
        return true;
#else
        // eFuse-corrected single-shot mV reading (no oversampling needed
        // for diagnostics; raw+Kalman path is used for normalized flex).
        for (uint8_t i = 0; i < 5; i++) {
            out[i] = analogReadMilliVolts(kPins[i]);
        }
        return true;
#endif
    }

    // --- Calibration (NVS persistence, D-ADC-3) ---

    bool persistCalibration() override {
#ifndef UNIT_TEST
        _prefs.begin("flex_cal", false);  // read-write
        _prefs.putBytes("L_min", _min, sizeof(_min));
        _prefs.putBytes("L_max", _max, sizeof(_max));
        _prefs.putUChar("ver", kCalibVersion);
        _prefs.end();
        _has_calib = true;
        Serial.println("[InternalADC] calibration persisted to NVS");
#else
        // Native simulation: mark as persisted (no real NVS)
        _has_calib = true;
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

    // --- Test hooks (native only) ---
#ifdef UNIT_TEST
    void setSimRaw(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _sim_raw[i] = vals[i];
    }
    void setSimMV(const uint16_t vals[5]) {
        for (int i = 0; i < 5; i++) _sim_mv[i] = vals[i];
    }
    void setCalibrationForTest(const uint16_t mn[5], const uint16_t mx[5]) {
        for (int i = 0; i < 5; i++) { _min[i] = mn[i]; _max[i] = mx[i]; }
        _has_calib = true;
    }
#endif

private:
    bool      _initialized;
    bool      _has_calib;
    uint16_t  _min[5];
    uint16_t  _max[5];

#ifdef UNIT_TEST
    uint16_t  _sim_raw[5] = {2048, 2048, 2048, 2048, 2048};
    uint16_t  _sim_mv[5]  = {1650, 1650, 1650, 1650, 1650};
#else
    Preferences           _prefs;
    KalmanFilter1D<float>* _kalman[5] = {nullptr};
#endif

    void _loadCalibration() {
#ifndef UNIT_TEST
        if (_prefs.isKey("L_min") && _prefs.isKey("L_max")) {
            _prefs.getBytes("L_min", _min, sizeof(_min));
            _prefs.getBytes("L_max", _max, sizeof(_max));
            _has_calib = true;
        } else {
            _has_calib = false;
        }
#endif
    }
};

// Static member definitions (required pre-C++17 inline for some toolchains)
constexpr uint8_t  InternalADCManager::kPins[5];
constexpr uint8_t  InternalADCManager::kOversample;
constexpr uint16_t InternalADCManager::kRawMinDef;
constexpr uint16_t InternalADCManager::kRawMaxDef;
constexpr uint8_t  InternalADCManager::kCalibVersion;
