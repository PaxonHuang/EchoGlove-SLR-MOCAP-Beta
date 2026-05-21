/* =============================================================================
 * EdgeAI Data Glove V3 — TCA9548A / PCA9548A I2C Multiplexer Driver
 * =============================================================================
 * Header-only convenience wrapper around the 8-channel I2C mux.
 * All 5 TMAG5273 Hall sensors share one I2C bus through this mux.
 * The BNO085 IMU sits on a dedicated mux channel (ch5).
 *
 * NOTE: PCA9548A (NXP) and TCA9548A (TI) are register-compatible.
 * This driver works with both. Actual hardware: PCA9548A breakout.
 *
 * CRITICAL USAGE PATTERN:
 *   1. Call disableAll() first
 *   2. Then call selectChannel(ch)
 *   The two-step approach prevents bus contention when switching between
 *   channels — the V2 code only called selectChannel() and suffered
 *   intermittent I2C lockups.
 *
 * Hardware: PCA9548A, default address 0x70, 3.3V, 400 kHz I2C
 * =============================================================================
 */

#ifndef TCA9548A_H
#define TCA9548A_H

#include <Arduino.h>
#include <Wire.h>

class TCA9548A {
public:
    /// Default I2C address (A0-A2 all grounded)
    static constexpr uint8_t DEFAULT_ADDR = 0x70;

    // =========================================================================
    // Construction
    // =========================================================================

    explicit TCA9548A(uint8_t address = DEFAULT_ADDR, TwoWire* wire = &Wire)
        : _addr(address), _wire(wire), _currentChannel(0xFF) {}

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the multiplexer — verify I2C presence, disable all channels.
     * @return true if device responds on the bus.
     */
    bool begin() {
        _wire->beginTransmission(_addr);
        uint8_t error = _wire->endTransmission();
        if (error != 0) {
            Serial.printf("[TCA9548A] ERROR: No device at 0x%02X (err=%d)\n", _addr, error);
            return false;
        }
        disableAll();
        Serial.printf("[TCA9548A] Initialized at 0x%02X\n", _addr);
        return true;
    }

    // =========================================================================
    // Channel Selection
    // =========================================================================

    /**
     * @brief Disable ALL channels (safe idle state).
     *
     * This isolates every downstream device from the I2C master bus.
     * Call this before selectChannel() to avoid bus contention.
     */
    void disableAll() {
        _wire->beginTransmission(_addr);
        _wire->write(0x00);  // All channels off
        _wire->endTransmission();
        _currentChannel = 0xFF;
    }

    /**
     * @brief Enable a single channel and disable all others.
     *
     * IMPORTANT: This method calls disableAll() internally BEFORE enabling
     * the requested channel. This two-step sequence ensures no two channels
     * are ever active simultaneously, preventing I2C bus collisions.
     *
     * @param ch  Channel number (0–7). Values outside range are clamped.
     */
    void selectChannel(uint8_t ch) {
        // Step 1: Disable all channels first (CRITICAL — prevents contention)
        disableAll();

        // Step 2: Clamp to valid range
        if (ch > 7) {
            ch = 0;
        }

        // Step 3: Enable the requested channel
        _wire->beginTransmission(_addr);
        _wire->write(1 << ch);
        _wire->endTransmission();
        _currentChannel = ch;

        // Step 4: Mandatory 1 ms delay for bus stability
        // The downstream sensor needs time to respond to the newly-routed bus.
        // Without this delay, the first read after a channel switch can return
        // stale data or NACK.
        delay(1);
    }

    // =========================================================================
    // Query
    // =========================================================================

    /** @return Currently active channel, or 0xFF if none/disabled. */
    uint8_t getActiveChannel() const { return _currentChannel; }

    /**
     * @brief Check if a device is present on a specific channel.
     * Temporarily switches to that channel, probes, then disables.
     * @param ch        Channel to scan.
     * @param devAddr   I2C address of the expected downstream device.
     * @return true if device ACKs on that channel.
     */
    bool probeChannel(uint8_t ch, uint8_t devAddr) {
        selectChannel(ch);
        _wire->beginTransmission(devAddr);
        uint8_t err = _wire->endTransmission();
        disableAll();
        return (err == 0);
    }

private:
    uint8_t  _addr;           ///< Mux I2C address
    TwoWire* _wire;           ///< I2C bus pointer
    uint8_t  _currentChannel; ///< Active channel (0xFF = none)
};

#endif // TCA9548A_H
