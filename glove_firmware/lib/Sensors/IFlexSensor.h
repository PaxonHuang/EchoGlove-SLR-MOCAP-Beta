/* =============================================================================
 * EchoGlove V6 — IFlexSensor Interface (Strategy/Adapter)
 * =============================================================================
 * Abstraction for the 5-channel flex sensor source.
 *
 * Implementations:
 *   - InternalADCManager  (V6 default — ESP32-S3 internal ADC1, GPIO1-5)
 *   - ADS1115FlexAdapter  (V5 compat — wraps ADS1115Manager)
 *
 * Design rationale: see docs/V6/07_internal_adc_migration.md §4 (D-ADC-4).
 * The interface lets the flex source be swapped without touching FlexManager,
 * the normalizer, the inference pipeline, or the packet encoder.
 * =============================================================================
 */

#pragma once
#include <cstdint>

class IFlexSensor {
public:
    virtual ~IFlexSensor() = default;

    // Initialize hardware and load NVS calibration (if applicable).
    // Returns true on success.
    virtual bool begin() = 0;

    // Read raw ADC counts (12-bit range: 0-4095 for internal ADC1).
    // out[0..4] = Thumb, Index, Middle, Ring, Pinky.
    // Returns true on success, false on hardware error.
    virtual bool readRaw(uint16_t out[5]) = 0;

    // Read eFuse-corrected millivolts (for diagnostics).
    // out[0..4] = Thumb..Pinky in mV (0-3300).
    virtual bool readMilliVolts(uint16_t out[5]) = 0;

    // Persist calibration (raw min/max) to NVS. Default: no-op (success).
    virtual bool persistCalibration() { return true; }

    // True if calibration data was loaded from NVS at begin().
    virtual bool hasCalibration() const { return false; }

    // Get current calibration bounds (raw counts). Default: full 12-bit range.
    virtual void getCalibrationBounds(uint16_t minOut[5], uint16_t maxOut[5]) const {
        for (int i = 0; i < 5; i++) {
            minOut[i] = 0;
            maxOut[i] = 4095;
        }
    }
};
