/* =============================================================================
 * EdgeAI Data Glove V3 — Kalman Filter 1D (Header-Only)
 * =============================================================================
 * Lightweight single-state Kalman filter for real-time sensor smoothing.
 * Applied to all 21 signals (15 Hall + 6 IMU) before feature extraction.
 *
 * Template Parameters:
 *   T — floating-point type (float for embedded, double if needed)
 *
 * V3 Note:
 *   In V2, KalmanFilter was applied post-hoc and could diverge on first
 *   boot due to uninitialized state. V3 auto-initializes the filter on
 *   the very first update() call by using the measurement as the initial
 *   estimate, preventing startup transients.
 *
 * Usage:
 *   KalmanFilter1D<float> kf(0.001f, 0.01f);  // Q=process, R=measurement noise
 *   float filtered = kf.update(raw_value);
 * =============================================================================
 */

#ifndef KALMAN_FILTER_1D_H
#define KALMAN_FILTER_1D_H

#include <cmath>
#include <algorithm>
#include <vector>

template <typename T = float>
class KalmanFilter1D {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a 1D Kalman filter.
     * @param process_noise   Q — estimate of measurement process noise variance.
     *                        Larger Q → filter tracks faster, less smoothing.
     *                        Typical: 0.0001 – 0.01
     * @param measurement_noise R — estimate of sensor measurement noise variance.
     *                          Larger R → filter trusts sensor less, more smoothing.
     *                          Typical: 0.001 – 0.1
     */
    /**
     * @brief Single-channel constructor (original API).
     */
    explicit KalmanFilter1D(T process_noise = T(0.001), T measurement_noise = T(0.01))
        : Q(process_noise), R(measurement_noise),
          x(T(0)),       // State estimate
          P(T(1)),       // Estimate error covariance
          K(T(0)),       // Kalman gain
          _initialized(false),
          _num_channels(0) {}

    /**
     * @brief Multi-channel constructor.
     *
     * Creates N independent Kalman filters sharing the same Q/R tuning.
     * Use the batch update(const T* input, T* output) method.
     *
     * @param num_channels   Number of independent signal channels.
     * @param process_noise  Q — process noise variance (shared across channels).
     * @param measurement_noise R — measurement noise variance (shared across channels).
     */
    explicit KalmanFilter1D(int num_channels,
                            T process_noise = T(0.001),
                            T measurement_noise = T(0.01))
        : Q(process_noise), R(measurement_noise),
          x(T(0)), P(T(1)), K(T(0)), _initialized(false),
          _num_channels(static_cast<size_t>(num_channels)),
          _mc_state(static_cast<size_t>(num_channels), T(0)),
          _mc_cov(static_cast<size_t>(num_channels), T(1)),
          _mc_init(static_cast<size_t>(num_channels), false) {}

    // =========================================================================
    // Core Filter Operation
    // =========================================================================

    /**
     * @brief Run one Kalman update cycle.
     *
     * Prediction step (for 1D constant model):
     *   x̂⁻ = x̂  (no velocity term in 1D)
     *   P⁻  = P + Q
     *
     * Update step:
     *   K   = P⁻ / (P⁻ + R)
     *   x̂   = x̂⁻ + K × (z - x̂⁻)
     *   P   = (1 - K) × P⁻
     *
     * @param measurement  Raw sensor reading.
     * @return Filtered estimate.
     */
    T update(T measurement) {
        if (!_initialized) {
            // First call: seed the filter with the actual measurement
            // to avoid large initial transient / divergence
            x = measurement;
            P = R;  // Start with measurement noise as initial uncertainty
            _initialized = true;
            return x;
        }

        // ---- Prediction ----
        // State does not change (constant model: x_next = x)
        T P_pred = P + Q;

        // ---- Update ----
        K = P_pred / (P_pred + R);
        x = x + K * (measurement - x);
        P = (T(1) - K) * P_pred;

        return x;
    }

    /**
     * @brief Run one Kalman update cycle on all channels (multi-channel mode).
     *
     * Each channel is an independent 1D Kalman filter sharing the same Q/R.
     * On the first call per channel, the filter seeds itself from the measurement.
     *
     * @param input   Pointer to array of num_channels raw measurements.
     * @param output  Pointer to array of num_channels filtered estimates (written).
     */
    void update(const T* input, T* output) {
        for (size_t i = 0; i < _num_channels; ++i) {
            if (!_mc_init[i]) {
                _mc_state[i] = input[i];
                _mc_cov[i]  = R;
                _mc_init[i] = true;
                output[i]   = _mc_state[i];
                continue;
            }
            // Prediction
            T P_pred = _mc_cov[i] + Q;
            // Update
            T k = P_pred / (P_pred + R);
            _mc_state[i] = _mc_state[i] + k * (input[i] - _mc_state[i]);
            _mc_cov[i]   = (T(1) - k) * P_pred;
            output[i]    = _mc_state[i];
        }
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /** @return Current filtered state estimate. */
    T getEstimate() const { return x; }

    /** @return Current error covariance (uncertainty). */
    T getErrorCovariance() const { return P; }

    /** @return Current Kalman gain. */
    T getKalmanGain() const { return K; }

    /** @return true if the filter has been seeded with at least one measurement. */
    bool isInitialized() const { return _initialized; }

    /** @return Number of channels (0 = single-channel mode). */
    size_t numChannels() const { return _num_channels; }

    // =========================================================================
    // Tuning
    // =========================================================================

    /** @brief Adjust process noise Q at runtime. */
    void setProcessNoise(T q) { Q = std::max(T(1e-8), q); }

    /** @brief Adjust measurement noise R at runtime. */
    void setMeasurementNoise(T r) { R = std::max(T(1e-8), r); }

    // =========================================================================
    // Reset
    // =========================================================================

    /**
     * @brief Reset filter to uninitialized state.
     * Next update() call will re-seed from the measurement.
     */
    void reset() {
        x = T(0);
        P = T(1);
        K = T(0);
        _initialized = false;
        // Also reset multi-channel state if applicable
        std::fill(_mc_state.begin(), _mc_state.end(), T(0));
        std::fill(_mc_cov.begin(),   _mc_cov.end(),   T(1));
        std::fill(_mc_init.begin(),  _mc_init.end(),  false);
    }

    /**
     * @brief Reset filter with a known initial estimate.
     * @param initial_value  Starting state estimate.
     * @param initial_p      Starting error covariance (default = R).
     */
    void resetTo(T initial_value, T initial_p = T(0)) {
        x = initial_value;
        P = (initial_p > T(0)) ? initial_p : R;
        K = T(0);
        _initialized = true;
    }

private:
    T Q;                  ///< Process noise variance
    T R;                  ///< Measurement noise variance
    T x;                  ///< State estimate (filtered output)
    T P;                  ///< Estimate error covariance
    T K;                  ///< Kalman gain (stored for diagnostics)
    bool _initialized;    ///< First-measurement guard

    // --- Multi-channel state (empty vectors in single-channel mode) ---
    size_t        _num_channels;  ///< 0 = single-channel, >0 = multi-channel
    std::vector<T>    _mc_state;  ///< Per-channel state estimates
    std::vector<T>    _mc_cov;    ///< Per-channel error covariances
    std::vector<bool> _mc_init;   ///< Per-channel initialization flags
};

#endif // KALMAN_FILTER_1D_H
