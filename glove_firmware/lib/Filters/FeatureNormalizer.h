/* =============================================================================
 * EdgeAI Data Glove V3 — Feature Normalization
 * =============================================================================
 * Min-Max normalization for sensor features.
 * Each feature channel has independent min/max bounds.
 *
 * Usage pattern:
 *   1. Calibration phase (2s @ 100Hz = 200 frames): call updateStats() on
 *      each frame to collect running min/max per channel.
 *   2. Inference phase: call normalize() to map features to [0, 1].
 *
 * Note: For quaternion-derived euler angles, the range is naturally [-180, 180].
 *       For gyro, range is [-max_gyro, max_gyro].
 *       These are handled by the per-channel min/max approach.
 * =============================================================================
 */

#ifndef FEATURE_NORMALIZER_H
#define FEATURE_NORMALIZER_H

#include "data_structures.h"
#include <cstring>
#include <cfloat>
#include <algorithm>

class FeatureNormalizer {
public:
    FeatureNormalizer() : _calibrated(false), _sample_count(0) {
        reset();
    }

    /** @brief Reset all statistics to uncalibrated state. */
    void reset() {
        for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
            _min[i] = FLT_MAX;
            _max[i] = -FLT_MAX;
        }
        _calibrated = false;
        _sample_count = 0;
    }

    /**
     * @brief Update running min/max statistics with a new sample.
     * @param features  Array of FEATURE_COUNT floats (Kalman-filtered).
     */
    void updateStats(const float* features) {
        for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
            if (features[i] < _min[i]) _min[i] = features[i];
            if (features[i] > _max[i]) _max[i] = features[i];
        }
        _sample_count++;
        _calibrated = (_sample_count > 0);
    }

    /**
     * @brief Normalize features to [0, 1] using stored min/max.
     * @param features  Input/output array of FEATURE_COUNT floats.
     * @param epsilon   Prevent division by zero for zero-variance channels.
     */
    void normalize(float* features, float epsilon = 1e-6f) {
        if (!_calibrated) return;

        for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
            float range = _max[i] - _min[i];
            if (range < epsilon) {
                features[i] = 0.5f;  // No variance → center at 0.5
            } else {
                features[i] = (features[i] - _min[i]) / range;
                features[i] = std::max(0.0f, std::min(1.0f, features[i]));
            }
        }
    }

    /** @return true if statistics have been collected. */
    bool isCalibrated() const { return _calibrated; }

    /** @return Number of samples used for calibration. */
    uint32_t getSampleCount() const { return _sample_count; }

    /** @return Range (max - min) for a specific feature channel. */
    float getRange(uint8_t channel) const {
        if (channel >= FEATURE_COUNT || !_calibrated) return 0.0f;
        return _max[channel] - _min[channel];
    }

    /** @return Min value for a specific feature channel. */
    float getMin(uint8_t channel) const {
        if (channel >= FEATURE_COUNT) return 0.0f;
        return _min[channel];
    }

    /** @return Max value for a specific feature channel. */
    float getMax(uint8_t channel) const {
        if (channel >= FEATURE_COUNT) return 0.0f;
        return _max[channel];
    }

private:
    float    _min[FEATURE_COUNT];    ///< Per-channel minimum values
    float    _max[FEATURE_COUNT];    ///< Per-channel maximum values
    bool     _calibrated;            ///< Statistics collected flag
    uint32_t _sample_count;          ///< Number of calibration samples
};

#endif // FEATURE_NORMALIZER_H