// ch0 deep scan — check ALL addresses for TMAG5273
#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define MUX_ADDR 0x70

void selectChannel(uint8_t ch) {
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(1 << ch);
    Wire.endTransmission();
}

void disableAll() {
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(0x00);
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== ch0 Deep Scan (all addresses) ===\n");

    Wire.begin(SDA_PIN, SCL_PIN, 100000);
    delay(100);

    // Verify MUX first
    Wire.beginTransmission(MUX_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("ERROR: PCA9548A not found on main bus!");
        return;
    }
    Serial.println("PCA9548A OK on main bus\n");

    // Select ch0
    selectChannel(0);
    delay(50);

    Serial.println("Scanning ch0 (TMAG5273) at 100kHz:");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  [0x%02X] ACK", addr);
            if (addr == 0x70) Serial.print("  (MUX itself — normal)");
            if (addr == 0x22) Serial.print("  ← TMAG5273 DEFAULT!");
            if (addr == 0x35) Serial.print("  ← TMAG5273 alt addr?");
            if (addr == 0x4B) Serial.print("  ← BNO085?");
            Serial.println();
            found++;
        }
    }
    if (found == 0) Serial.println("  (no devices at all)");

    // Also try at 400kHz
    disableAll();
    Wire.end();
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    delay(50);
    selectChannel(0);
    delay(50);

    Serial.println("\nScanning ch0 at 400kHz:");
    found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  [0x%02X] ACK", addr);
            if (addr == 0x70) Serial.print("  (MUX)");
            Serial.println();
            found++;
        }
    }
    if (found == 0) Serial.println("  (no devices)");

    disableAll();

    // Now scan ch5 for comparison
    Wire.end();
    Wire.begin(SDA_PIN, SCL_PIN, 100000);
    delay(50);
    selectChannel(5);
    delay(50);

    Serial.println("\nScanning ch5 (BNO085) for comparison:");
    found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  [0x%02X] ACK", addr);
            if (addr == 0x4B) Serial.print("  ← BNO085");
            if (addr == 0x70) Serial.print("  (MUX)");
            Serial.println();
            found++;
        }
    }
    if (found == 0) Serial.println("  (no devices)");

    disableAll();

    Serial.println("\n=== Conclusion ===");
    Serial.println("If ch0 shows only 0x70 (MUX), TMAG5273 is likely dead.");
    Serial.println("If ch0 shows 0x22 or other addr, note the address.");
}

void loop() {}
