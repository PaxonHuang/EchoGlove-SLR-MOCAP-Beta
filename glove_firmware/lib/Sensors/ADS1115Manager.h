/* =============================================================================
 * EchoGlove V5 — ADS1115 ADC Manager
 * =============================================================================
 * Manages two ADS1115 16-bit ADCs over I2C to read 5 flex sensors.
 * Supports simulation mode for unit testing without hardware.
 *
 * Channel mapping:
 *   ADS1115_ADDR_GND (0x48): AIN0=Thumb, AIN1=Index, AIN2=Middle
 *   ADS1115_ADDR_VDD (0x49): AIN0=Ring,  AIN1=Pinky
 *
 * I2C: SDA=GPIO8, SCL=GPIO9, 400kHz (flat bus, no MUX)
 * =============================================================================
 */

#pragma once
#include "data_structures.h"
#include <cstdint>

#ifndef UNIT_TEST
#include <Wire.h>
#endif

#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49
#define ADS1115_REG_CONV  0x00
#define ADS1115_REG_CFG   0x01

// ADS1115 config bits
// PGA = ±4.096V (FSR=4.096V, 0.125mV/count)
// Mode = single-shot
// Data rate = 128 SPS
// Comparator = disabled
#define ADS1115_CFG_BASE  0x0183  // [14:12]=MUX, [11:9]=PGA, [8]=MODE, [7:5]=DR, [4:2]=COMP
#define ADS1115_OS_START  0x8000  // OS=1: start single conversion

class ADS1115Manager {
public:
    bool begin(bool simulation = false) {
        simulation_ = simulation;
        if (simulation_) return true;

#ifndef UNIT_TEST
        Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
        Wire.setTimeOut(50);  // 50ms timeout per transaction (prevents hangs)
        Serial.printf("[ADS1115] I2C bus init: SDA=%d SCL=%d %dkHz\n",
                      I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ / 1000);
#endif
        return true;
    }

    bool readAll(float* values) {
        if (simulation_) {
            for (int i = 0; i < 5; i++) values[i] = sim_values_[i];
            return true;
        }
        // Read raw ADC values for debugging
        int16_t raw[5];
        raw[0] = readChannelRaw(ADS1115_ADDR_GND, 0); // Thumb
        raw[1] = readChannelRaw(ADS1115_ADDR_GND, 1); // Index
        raw[2] = readChannelRaw(ADS1115_ADDR_GND, 2); // Middle
        raw[3] = readChannelRaw(ADS1115_ADDR_VDD, 0); // Ring
        raw[4] = readChannelRaw(ADS1115_ADDR_VDD, 1); // Pinky

        // Debug: print raw ADC values every 100 calls
        static int call_count = 0;
        if (++call_count % 100 == 0) {
            Serial.printf("[ADS1115] raw=[%d %d %d %d %d]\n",
                          raw[0], raw[1], raw[2], raw[3], raw[4]);
        }

        // Normalize to [0, 1]
        for (int i = 0; i < 5; i++) {
            values[i] = normalize(raw[i]);
        }
        return true;
    }

    // Read raw 16-bit ADC value (for calibration/debugging)
    int16_t readRaw(uint8_t addr, int channel) {
        return readChannelRaw(addr, channel);
    }

    void setSimulatedValues(const float* values) {
        for (int i = 0; i < 5; i++) sim_values_[i] = values[i];
    }

private:
    bool simulation_ = false;
    float sim_values_[5] = {0};

    // Write 16-bit config to ADS1115, wait for conversion, read result
    int16_t readChannelRaw(uint8_t addr, int channel) {
#ifndef UNIT_TEST
        // Build config word: OS=1 (start), MUX=AINx-vs-GND, PGA=±4.096V, MODE=single
        uint16_t config = ADS1115_OS_START | ADS1115_CFG_BASE | ((uint16_t)(4 + channel) << 12);
        // MUX: 100=AIN0/GND, 101=AIN1/GND, 110=AIN2/GND → (4+channel) << 12

        // Write config register
        Wire.beginTransmission(addr);
        Wire.write(ADS1115_REG_CFG);
        Wire.write((uint8_t)(config >> 8));
        Wire.write((uint8_t)(config & 0xFF));
        uint8_t err = Wire.endTransmission();
        if (err != 0) {
            Serial.printf("[ADS1115] CFG write FAIL: addr=0x%02X ch=%d err=%d\n", addr, channel, err);
            return 0;
        }

        // Wait for conversion (~8ms at 128 SPS, use 12ms for safety)
        delay(12);

        // Point to conversion register
        Wire.beginTransmission(addr);
        Wire.write(ADS1115_REG_CONV);
        err = Wire.endTransmission();
        if (err != 0) {
            Serial.printf("[ADS1115] CONV pointer FAIL: addr=0x%02X ch=%d err=%d\n", addr, channel, err);
            return 0;
        }

        // Read 2 bytes
        uint8_t got = Wire.requestFrom(addr, (uint8_t)2);
        if (got >= 2) {
            int16_t raw = (Wire.read() << 8) | Wire.read();
            return raw;
        }
        Serial.printf("[ADS1115] READ FAIL: addr=0x%02X ch=%d got=%d\n", addr, channel, got);
#endif
        return 0;
    }

    // Normalize raw ADC to [0.0, 1.0]
    // PGA ±4.096V → 0.125mV/count
    // Flex sensor voltage divider: ~0.1V (straight) to ~3.2V (bent)
    float normalize(int16_t raw) {
        float f = (float)raw;
        float n = (f - RAW_MIN) / (RAW_MAX - RAW_MIN);
        if (n < 0.0f) n = 0.0f;
        if (n > 1.0f) n = 1.0f;
        return n;
    }

    // Flex sensor ADC range (will be refined during calibration)
    static constexpr float RAW_MIN = 260.0f;   // ~0.1V (sensor straight)
    static constexpr float RAW_MAX = 25600.0f;  // ~3.2V (sensor bent)
};
