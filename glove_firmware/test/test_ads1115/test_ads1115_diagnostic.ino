/* =============================================================================
 * GY-ADS1115 Diagnostic Firmware — ESP32-S3-DevKitC-1 N16R8
 * =============================================================================
 * Purpose: Verify ADS1115 ADC modules are detected on I2C bus
 *
 * Wiring (ADS1115 Module 1 — Address 0x48):
 *   VCC → 3.3V (NOT 5V!)
 *   GND → GND
 *   SDA → GPIO8 (shared with BNO085)
 *   SCL → GPIO9 (shared with BNO085)
 *   ADDR → GND (selects address 0x48)
 *
 * Wiring (ADS1115 Module 2 — Address 0x49):
 *   VCC → 3.3V (NOT 5V!)
 *   GND → GND
 *   SDA → GPIO8 (shared with BNO085)
 *   SCL → GPIO9 (shared with BNO085)
 *   ADDR → 3.3V (selects address 0x49)
 *
 * Expected output:
 *   [I2C] Scanning 0x01..0x7F...
 *   [I2C] Found device at 0x4B (BNO085)
 *   [I2C] Found device at 0x48 (ADS1115 GND)
 *   [I2C] Found device at 0x49 (ADS1115 VDD)
 *   [ADS1115] 0x48 config=0x8583 (OK)
 *   [ADS1115] 0x49 config=0x8583 (OK)
 *
 * Build: pio run -e ads1115-diag -t upload
 * Monitor: pio device monitor -e ads1115-diag
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// I2C pins (same as BNO085)
#define SDA_PIN 8
#define SCL_PIN 9

// ADS1115 addresses
#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49

// ADS1115 registers
#define ADS1115_REG_CONV  0x00
#define ADS1115_REG_CFG   0x01

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("ADS1115 Diagnostic — ESP32-S3 N16R8");
    Serial.println("========================================\n");

    // Step 1: I2C scan
    Serial.println("[I2C] Scanning 0x01..0x7F...");
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);  // 100kHz for stability

    int deviceCount = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        byte err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("[I2C] Found device at 0x%02X", addr);
            if (addr == 0x4B) Serial.print(" (BNO085)");
            else if (addr == 0x48) Serial.print(" (ADS1115 GND)");
            else if (addr == 0x49) Serial.print(" (ADS1115 VDD)");
            Serial.println();
            deviceCount++;
        }
    }
    Serial.printf("[I2C] Total devices found: %d\n\n", deviceCount);

    // Step 2: Read ADS1115 config registers
    Serial.println("[ADS1115] Reading config registers...");

    // Check ADS1115@0x48
    Wire.beginTransmission(ADS1115_ADDR_GND);
    Wire.write(ADS1115_REG_CFG);
    Wire.endTransmission();
    Wire.requestFrom(ADS1115_ADDR_GND, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint16_t cfg = (Wire.read() << 8) | Wire.read();
        Serial.printf("[ADS1115] 0x48 config=0x%04X", cfg);
        if (cfg == 0x8583) Serial.println(" (OK — default config)");
        else Serial.printf(" (unexpected: 0x%04X)\n", cfg);
    } else {
        Serial.println("[ADS1115] 0x48 config read FAIL (no response)");
    }

    // Check ADS1115@0x49
    Wire.beginTransmission(ADS1115_ADDR_VDD);
    Wire.write(ADS1115_REG_CFG);
    Wire.endTransmission();
    Wire.requestFrom(ADS1115_ADDR_VDD, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint16_t cfg = (Wire.read() << 8) | Wire.read();
        Serial.printf("[ADS1115] 0x49 config=0x%04X", cfg);
        if (cfg == 0x8583) Serial.println(" (OK — default config)");
        else Serial.printf(" (unexpected: 0x%04X)\n", cfg);
    } else {
        Serial.println("[ADS1115] 0x49 config read FAIL (no response)");
    }

    // Step 3: Summary
    Serial.println("\n========================================");
    Serial.println("Diagnostic Complete");
    Serial.println("========================================");
    Serial.println("Expected: 3 devices (0x48, 0x49, 0x4B)");
    Serial.printf("Found: %d devices\n", deviceCount);
    if (deviceCount >= 3) {
        Serial.println("Status: PASS — All I2C devices detected");
    } else {
        Serial.println("Status: FAIL — Missing devices, check wiring");
    }
    Serial.println("========================================\n");
}

void loop() {
    // Diagnostic runs once, then idle
    delay(10000);
}
