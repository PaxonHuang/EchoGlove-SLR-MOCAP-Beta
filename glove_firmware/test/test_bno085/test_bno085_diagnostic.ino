/*
 * BNO085 Diagnostic Test — I2C Scan + RST Pin Check
 *
 * Wiring (ESP32-S3-DevKitC-1 N16R8):
 *   VCC → 3.3V
 *   GND → GND
 *   SDA → GPIO8
 *   SCL → GPIO9
 *   CS  → 3.3V (HIGH for I2C mode)
 *   PS0 → GND (I2C mode)
 *   PS1 → GND (I2C mode)
 *   ADO → 3.3V (address 0x4B)
 *   RST → GPIO10 (or 3.3V if not needed)
 *
 * Expected output:
 *   - I2C scan finds 0x4B
 *   - RST pin state check
 *   - Recommendations if issues detected
 *
 * History:
 *   2026-06-21: Initial diagnostic — PS0/PS1 wiring correction, GPIO8/9 on N16R8
 */

#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define RST_PIN 10
#define BNO_ADDR 0x4B

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  BNO085 Diagnostic Test");
    Serial.println("========================================\n");

    // --- I2C Scan ---
    Serial.println("[1] I2C scan...");
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    bool found = false;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found: 0x%02X", addr);
            if (addr == BNO_ADDR) Serial.print(" [BNO085]");
            Serial.println();
            found = true;
        }
    }
    if (!found) {
        Serial.println("[FAIL] No I2C devices found!");
        Serial.println("Check wiring: SDA=GPIO8, SCL=GPIO9");
        while (1) delay(1000);
    }

    // --- RST Pin Check ---
    Serial.println("\n[2] RST pin check (GPIO10):");
    pinMode(RST_PIN, INPUT);
    int rstState = digitalRead(RST_PIN);
    Serial.printf("  GPIO10 = %s\n", rstState ? "HIGH" : "LOW");

    pinMode(RST_PIN, INPUT_PULLUP);
    int rstPullup = digitalRead(RST_PIN);
    Serial.printf("  GPIO10 (with pull-up) = %s\n", rstPullup ? "HIGH" : "LOW");

    pinMode(RST_PIN, INPUT_PULLDOWN);
    int rstPulldown = digitalRead(RST_PIN);
    Serial.printf("  GPIO10 (with pull-down) = %s\n", rstPulldown ? "HIGH" : "LOW");

    if (rstState == HIGH && rstPullup == HIGH && rstPulldown == HIGH) {
        Serial.println("  [WARN] RST is HARD HIGH — likely connected to 3.3V!");
        Serial.println("         GPIO10 cannot pull it LOW for reset.");
    } else {
        Serial.println("  [OK] RST is controllable");
    }

    // --- Recommendations ---
    Serial.println("\n========================================");
    Serial.println("  RECOMMENDATIONS");
    Serial.println("========================================");
    Serial.println("1. CHECK RST PIN:");
    Serial.println("   - If RST is connected to 3.3V AND GPIO10:");
    Serial.println("     → REMOVE 3.3V connection, keep only GPIO10");
    Serial.println("   - If RST is only connected to 3.3V:");
    Serial.println("     → Either: keep 3.3V (no hardware reset)");
    Serial.println("     → Or: connect to GPIO10 (for hardware reset)");
    Serial.println("\n2. POWER CYCLE BNO085:");
    Serial.println("   - Disconnect VCC for 10 seconds");
    Serial.println("   - Reconnect VCC");
    Serial.println("   - Run this test again");
    Serial.println("\n3. VERIFY PS0/PS1:");
    Serial.println("   - PS0 must be GND (not 3.3V!)");
    Serial.println("   - PS1 must be GND");
    Serial.println("   - CS must be 3.3V");
    Serial.println("   - ADO must be 3.3V (for 0x4B)");
    Serial.println("\nDone. Reset board to run again.");
}

void loop() {
    delay(1000);
}
