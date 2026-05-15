/* =============================================================================
 * EdgeAI Data Glove V3 — TMAG5273 3D Hall-Effect Sensor Driver
 * =============================================================================
 * Driver for the Texas Instruments TMAG5273A1 linear 3D Hall-effect sensor.
 * Provides 3-axis magnetic field measurement in millitesla (mT).
 *
 * Key Configuration:
 *   - Resolution: 12-bit (0.488 mT / LSB at ±40 mT range)
 *   - Averaging: 32× (CONFIG register AVG=0b11) for noise reduction
 *   - Operating mode: Set/Reset trigger (magnetic periodic wake, lowest power)
 *   - Conversion: Manual trigger via CONV register bit 4
 *
 * Datasheet: https://www.ti.com/lit/ds/symlink/tmag5273.pdf
 * Note: Register addresses differ from the SparkFun library defaults;
 * this driver uses the bare-metal TI register map.
 * =============================================================================
 */

#ifndef TMAG5273_H
#define TMAG5273_H

#include <Arduino.h>
#include <Wire.h>

// Forward declare the mux — we need it for bus arbitration but don't own it
class TCA9548A;

class TMAG5273 {
public:
    /// Default I2C address when A0=0, A1=0 (TMAG5273A1 variant)
    static constexpr uint8_t DEFAULT_ADDR = 0x22;

    /// Full-scale range (signed 12-bit → ±40 mT)
    static constexpr float FULL_SCALE_MT = 40.0f;

    /// 12-bit ADC resolution
    static constexpr float ADC_RESOLUTION = 4096.0f;  // 2^12

    /// Conversion factor: LSB → millitesla
    static constexpr float LSB_TO_MT = (2.0f * FULL_SCALE_MT) / ADC_RESOLUTION;  // ≈0.01953 mT/LSB

    // =========================================================================
    // TMAG5273 Register Map (TI native addresses)
    // =========================================================================
    struct Reg {
        static constexpr uint8_t DEVICE_ID   = 0x00;  // R, 0x01 = TMAG5273
        static constexpr uint8_t X_MSB       = 0x01;  // R, [11:4] of X
        static constexpr uint8_t X_LSB       = 0x02;  // R, [3:0]  of X
        static constexpr uint8_t Y_MSB       = 0x03;  // R, [11:4] of Y
        static constexpr uint8_t Y_LSB       = 0x04;  // R, [3:0]  of Y
        static constexpr uint8_t Z_MSB       = 0x05;  // R, [11:4] of Z
        static constexpr uint8_t Z_LSB       = 0x06;  // R, [3:0]  of Z
        static constexpr uint8_t T_CONV_MSB  = 0x07;  // R, [11:4] of temperature
        static constexpr uint8_t T_CONV_LSB  = 0x08;  // R, [3:0]  of temperature
        static constexpr uint8_t CONV        = 0x09;  // RW, conversion control
        static constexpr uint8_t INT_CONFIG  = 0x0A;  // RW, interrupt config
        static constexpr uint8_t THRESHOLD_X = 0x0B;  // RW, X threshold
        static constexpr uint8_t THRESHOLD_Y = 0x0C;  // RW, Y threshold
        static constexpr uint8_t THRESHOLD_Z = 0x0D;  // RW, Z threshold
        static constexpr uint8_t THRESHOLD_XYZ = 0x0E; // RW, magnitude threshold
        static constexpr uint8_t MAG_CONFIG  = 0x0F;  // RW, magnetic config
        static constexpr uint8_t I2C_ADDR    = 0x10;  // RW, I2C address
        static constexpr uint8_t T_CONFIG    = 0x11;  // RW, temperature config
        static constexpr uint8_t ANGLE_EN    = 0x12;  // RW, angle calculation en
        static constexpr uint8_t M_AXIS_SEL  = 0x13;  // RW, magnetic axis select
        static constexpr uint8_t SYS_CONFIG  = 0x14;  // RW, system config
        static constexpr uint8_t DEVICE_CONFIG = 0x15; // RW, device config
        static constexpr uint8_t SENS_CONFIG = 0x16;  // RW, sensitivity config
        static constexpr uint8_t ALERT_LOW   = 0x17;  // RW, low alert threshold
        static constexpr uint8_t ALERT_HIGH  = 0x18;  // RW, high alert threshold
    };

    // CONV register bits
    struct ConvBits {
        static constexpr uint8_t TRIGGER   = (1 << 4);  // Trigger single conversion
        static constexpr uint8_t NUM_AVG_1 = (0 << 0);  // 1× averaging
        static constexpr uint8_t NUM_AVG_4 = (1 << 0);  // 4× averaging
        static constexpr uint8_t NUM_AVG_16 = (2 << 0); // 16× averaging
        static constexpr uint8_t NUM_AVG_32 = (3 << 0); // 32× averaging
    };

    // MAG_CONFIG register bits
    struct MagConfigBits {
        static constexpr uint8_t RANGE_40MT = (1 << 4);  // ±40 mT
        static constexpr uint8_t RANGE_20MT = (0 << 4);  // ±20 mT
    };

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @param mux       Pointer to the TCA9548A multiplexer (must not be null).
     * @param mux_ch    Which mux channel this sensor sits on (0–7).
     * @param addr      I2C address (default 0x22 for TMAG5273A1).
     * @param wire      I2C bus pointer.
     */
    TMAG5273(TCA9548A* mux, uint8_t mux_ch,
             uint8_t addr = DEFAULT_ADDR, TwoWire* wire = &Wire)
        : _mux(mux), _mux_ch(mux_ch), _addr(addr), _wire(wire),
          _last_x(0.0f), _last_y(0.0f), _last_z(0.0f) {}

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the sensor with recommended V3 configuration.
     *
     * Configures:
     *   - 12-bit resolution (default)
     *   - ±40 mT range
     *   - 32× averaging (CONV register AVG bits)
     *   - Set/Reset trigger mode (lowest power for periodic reads)
     *
     * @return true if device ID matches (0x01) and config was written.
     */
    bool begin() {
        if (_mux == nullptr) {
            Serial.println("[TMAG5273] ERROR: Mux pointer is null!");
            return false;
        }

        // Switch to our mux channel
        _mux->selectChannel(_mux_ch);

        // Verify device ID
        uint8_t dev_id = readReg(Reg::DEVICE_ID);
        if (dev_id != 0x01) {
            Serial.printf("[TMAG5273] ERROR: Wrong Device ID 0x%02X on mux ch%d (expected 0x01)\n",
                          dev_id, _mux_ch);
            _mux->disableAll();
            return false;
        }

        // Configure CONV register: 32× averaging
        // Bits [1:0] = 0b11 for 32× averaging
        uint8_t conv_val = ConvBits::NUM_AVG_32;
        writeReg(Reg::CONV, conv_val);

        // Configure MAG_CONFIG: ±40 mT range
        uint8_t mag_val = readReg(Reg::MAG_CONFIG);
        mag_val |= MagConfigBits::RANGE_40MT;
        writeReg(Reg::MAG_CONFIG, mag_val);

        // Configure DEVICE_CONFIG for Set/Reset trigger mode
        // (ensures consistent readings by demagnetizing before each sample)
        uint8_t dev_val = readReg(Reg::DEVICE_CONFIG);
        dev_val |= (1 << 2);  // Enable set/reset
        writeReg(Reg::DEVICE_CONFIG, dev_val);

        // Release bus
        _mux->disableAll();

        Serial.printf("[TMAG5273] Sensor on mux ch%d initialized (addr=0x%02X, ±40mT, 32× avg)\n",
                      _mux_ch, _addr);
        return true;
    }

    // =========================================================================
    // Measurement
    // =========================================================================

    /**
     * @brief Trigger a new ADC conversion and read XYZ magnetic field.
     *
     * This method:
     *   1. Selects the mux channel
     *   2. Writes to CONV register to trigger conversion
     *   3. Waits for conversion complete (typical 6 ms with 32× averaging)
     *   4. Reads X_MSB/X_LSB, Y_MSB/Y_LSB, Z_MSB/Z_LSB
     *   5. Disables the mux channel
     *
     * @param x_out  Pointer to receive X-axis value (mT)
     * @param y_out  Pointer to receive Y-axis value (mT)
     * @param z_out  Pointer to receive Z-axis value (mT)
     * @return true if read succeeded.
     */
    bool readXYZ(float* x_out, float* y_out, float* z_out) {
        if (_mux == nullptr || x_out == nullptr ||
            y_out == nullptr || z_out == nullptr) {
            return false;
        }

        // Step 1: Select our mux channel (includes disableAll() internally)
        _mux->selectChannel(_mux_ch);

        // Step 2: Trigger a single conversion
        uint8_t conv_val = readReg(Reg::CONV);
        conv_val |= ConvBits::TRIGGER;
        writeReg(Reg::CONV, conv_val);

        // Step 3: Wait for conversion to complete
        // With 32× averaging at 12-bit, max conversion time ≈ 6.5 ms
        // We poll CONV register bit 4 (TRIGGER clears when done)
        uint32_t start = micros();
        while ((readReg(Reg::CONV) & ConvBits::TRIGGER) != 0) {
            if ((micros() - start) > 10000) {  // 10 ms timeout
                Serial.printf("[TMAG5273] Timeout on mux ch%d\n", _mux_ch);
                _mux->disableAll();
                return false;
            }
            delayMicroseconds(100);
        }

        // Step 4: Read X, Y, Z result registers
        // Each axis is 12-bit signed: MSB=[11:4], LSB=[3:0]
        int16_t raw_x = readAxis(Reg::X_MSB, Reg::X_LSB);
        int16_t raw_y = readAxis(Reg::Y_MSB, Reg::Y_LSB);
        int16_t raw_z = readAxis(Reg::Z_MSB, Reg::Z_LSB);

        // Step 5: Convert to millitesla
        _last_x = raw_x * LSB_TO_MT;
        _last_y = raw_y * LSB_TO_MT;
        _last_z = raw_z * LSB_TO_MT;

        // Step 6: Release mux channel
        _mux->disableAll();

        *x_out = _last_x;
        *y_out = _last_y;
        *z_out = _last_z;
        return true;
    }

    /**
     * @brief Trigger conversion only (no read). Useful for parallel trigger.
     */
    bool triggerConversion() {
        if (_mux == nullptr) return false;
        _mux->selectChannel(_mux_ch);
        uint8_t conv_val = readReg(Reg::CONV);
        conv_val |= ConvBits::TRIGGER;
        writeReg(Reg::CONV, conv_val);
        _mux->disableAll();
        return true;
    }

    /**
     * @brief Read temperature (in °C). Side effect: selects mux channel.
     * @return Temperature in Celsius, or NaN on error.
     */
    float readTemperature() {
        if (_mux == nullptr) return NAN;
        _mux->selectChannel(_mux_ch);

        int16_t raw_temp = readAxis(Reg::T_CONV_MSB, Reg::T_CONV_LSB);
        // Temperature: 25°C offset, ~0.25°C/LSB (simplified)
        float temp_c = 25.0f + (raw_temp * 0.25f);

        _mux->disableAll();
        return temp_c;
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    uint8_t getMuxChannel() const { return _mux_ch; }
    float getLastX() const { return _last_x; }
    float getLastY() const { return _last_y; }
    float getLastZ() const { return _last_z; }

private:
    TCA9548A* _mux;          ///< I2C multiplexer (not owned)
    uint8_t   _mux_ch;       ///< Mux channel for this sensor
    uint8_t   _addr;         ///< Sensor I2C address
    TwoWire*  _wire;         ///< I2C bus
    float     _last_x;       ///< Cached last reading
    float     _last_y;
    float     _last_z;

    // =========================================================================
    // Low-level I2C helpers
    // =========================================================================

    uint8_t readReg(uint8_t reg) {
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        _wire->endTransmission(false);  // Repeated start
        _wire->requestFrom(_addr, (uint8_t)1);
        if (_wire->available()) {
            return _wire->read();
        }
        return 0xFF;
    }

    void writeReg(uint8_t reg, uint8_t val) {
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        _wire->write(val);
        _wire->endTransmission();
    }

    /**
     * @brief Read a 12-bit signed axis value from two registers.
     * MSB contains bits [11:4], LSB contains bits [3:0] in upper nibble.
     * Sign-extended to int16_t for proper negative values.
     */
    int16_t readAxis(uint8_t msb_reg, uint8_t lsb_reg) {
        uint8_t msb = readReg(msb_reg);
        uint8_t lsb = readReg(lsb_reg);
        // Combine: MSB is [11:4], LSB upper nibble is [3:0]
        int16_t raw = (int16_t)((msb << 4) | (lsb >> 4));
        // Sign extend from 12-bit
        if (raw & 0x0800) {
            raw |= 0xF000;
        }
        return raw;
    }
};

#endif // TMAG5273_H
