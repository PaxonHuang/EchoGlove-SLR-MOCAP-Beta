/**
 * @file Sensors.h
 * @brief Sensor drivers for EchoGlove V5
 *
 * This directory contains:
 * - ADS1115: 16-bit ADC for 5 flex sensors (via I2C)
 * - BNO085: 9-axis IMU driver (SH-2 protocol)
 * - SensorManager: Unified sensor management
 *
 * V5 changes: Hall-effect sensors (TMAG5273) and TCA9548A mux
 * removed; flex sensors now read through ADS1115 ADC.
 */

#ifndef SENSORS_H
#define SENSORS_H

// Sensor module initialized - see individual driver files
#define SENSORS_MODULE_VERSION "5.0.0"

#endif // SENSORS_H
