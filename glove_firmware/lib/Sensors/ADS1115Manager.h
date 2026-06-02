/* =============================================================================
 * EchoGlove V5 — ADS1115 ADC Manager
 * =============================================================================
 * Manages two ADS1115 16-bit ADCs over I2C to read 5 flex sensors.
 * Supports simulation mode for unit testing without hardware.
 *
 * Channel mapping:
 *   ADS1115_ADDR_GND (0x48): AIN0=Thumb, AIN1=Index, AIN2=Middle
 *   ADS1115_ADDR_VDD (0x49): AIN0=Ring,  AIN1=Pinky
 * =============================================================================
 */

#pragma once
#include "data_structures.h"
#include <cstdint>

#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49
#define ADS1115_REG_CONV  0x00
#define ADS1115_REG_CFG   0x01

class ADS1115Manager {
public:
    bool begin(bool simulation = false) {
        simulation_ = simulation;
        if (simulation_) return true;
        // Real I2C: Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ)
        return true;
    }

    bool readAll(float* values) {
        if (simulation_) {
            for (int i = 0; i < 5; i++) values[i] = sim_values_[i];
            return true;
        }
        values[0] = readChannel(ADS1115_ADDR_GND, 0); // Thumb
        values[1] = readChannel(ADS1115_ADDR_GND, 1); // Index
        values[2] = readChannel(ADS1115_ADDR_GND, 2); // Middle
        values[3] = readChannel(ADS1115_ADDR_VDD, 0); // Ring
        values[4] = readChannel(ADS1115_ADDR_VDD, 1); // Pinky
        return true;
    }

    void setSimulatedValues(const float* values) {
        for (int i = 0; i < 5; i++) sim_values_[i] = values[i];
    }

private:
    bool simulation_ = false;
    float sim_values_[5] = {0};

    float readChannel(uint8_t addr, int channel) {
        // I2C: configure mux for channel, start conversion, read 16-bit result
        // Normalize: (raw - cal_min) / (cal_max - cal_min)
        return 0.0f; // placeholder for real hardware
    }
};
