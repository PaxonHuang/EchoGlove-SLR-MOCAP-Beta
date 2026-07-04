/* =============================================================================
 * EchoGlove V5 — I2C Device Isolation Test v1
 * =============================================================================
 *
 * Purpose: Step-by-step I2C isolation to identify which device is causing
 * Timeouts (err=5). Tests each device individually at both 100kHz and 400kHz.
 *
 * Test sequence:
 *   Step 1: Raw bus line check (SDA/SCL levels before any I2C init)
 *   Step 2: I2C scan @ 100kHz — who is on the bus?
 *   Step 3: I2C scan @ 400kHz — does faster speed work?
 *   Step 4: ADS1115 #1 (0x48) ONLY @ 100kHz — config write, read AIN0
 *   Step 5: ADS1115 #1 (0x48) ONLY @ 400kHz
 *   Step 6: ADS1115 #2 (0x49) ONLY @ 100kHz — config write, read AIN0
 *   Step 7: ADS1115 #2 (0x49) ONLY @ 400kHz
 *   Step 8: BNO085 (0x4B) ONLY @ 100kHz — begin_I2C, read sensors
 *   Step 9: BNO085 (0x4B) ONLY @ 400kHz
 *   Step 10: All 3 devices together @ 100kHz (ADS1115 #1 + #2 readings)
 *   Step 11: Bus stability — 10 reads of ADS1115 #1
 *
 * Wiring:
 *   SDA=GPIO8, SCL=GPIO9, 4.7kΩ pull-ups to 3.3V
 *   BNO085@0x4B, ADS1115#1@0x48, ADS1115#2@0x49
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

#define SDA_PIN     8
#define SCL_PIN     9
#define ADDR_ADS1   0x48
#define ADDR_ADS2   0x49
#define ADDR_BNO    0x4B

#define ADS1115_REG_CONV  0x00
#define ADS1115_REG_CFG   0x01

bool _ads_read(int addr, int channel, int16_t& raw) {
    uint16_t config = 0x8000 | 0x0183 | ((uint16_t)(4 + channel) << 12);
    Wire.beginTransmission(addr);
    Wire.write(ADS1115_REG_CFG);
    Wire.write((uint8_t)(config >> 8));
    Wire.write((uint8_t)(config & 0xFF));
    uint8_t err = Wire.endTransmission();
    if (err != 0) { Serial.printf("ERR cfg_wr=%d\n", err); return false; }
    delay(12);
    Wire.beginTransmission(addr);
    Wire.write(ADS1115_REG_CONV);
    err = Wire.endTransmission();
    if (err != 0) { Serial.printf("ERR ptr=%d\n", err); return false; }
    uint8_t n = Wire.requestFrom(addr, (uint8_t)2);
    if (n >= 2) {
        raw = (Wire.read() << 8) | Wire.read();
        return true;
    }
    Serial.printf("ERR read_short=%d\n", n);
    return false;
}

void scan_bus(int freq, const char* label) {
    Serial.printf("\n--- I2C Scan @ %d Hz (%s) ---\n", freq, label);
    Wire.begin(SDA_PIN, SCL_PIN, freq);
    Wire.setTimeOut(50);
    delay(10);

    int found = 0;
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            const char* lbl = (a == ADDR_ADS1) ? "ADS1115#1" :
                              (a == ADDR_ADS2) ? "ADS1115#2" :
                              (a == ADDR_BNO)  ? "BNO085" : "";
            Serial.printf("  [OK] 0x%02X %s\n", a, lbl);
            found++;
        } else if (err == 5) {
            Serial.printf("  [TO] 0x%02X TIMEOUT\n", a);
        }
        if ((a & 0x0F) == 0x07) {
            Serial.printf("  ...up to 0x%02X\n", a);
        }
    }
    Serial.printf("  Found: %d device(s)\n", found);
    Wire.end();
    delay(50);
}

void check_bus_lines() {
    pinMode(SDA_PIN, INPUT);
    pinMode(SCL_PIN, INPUT);
    delay(5);
    int sda = digitalRead(SDA_PIN);
    int scl = digitalRead(SCL_PIN);
    Serial.printf("Bus idle: SDA=%d SCL=%d (both should be HIGH)\n", sda, scl);
    if (!sda || !scl) {
        Serial.println("** WARNING: bus line(s) LOW — check pull-up resistors & devices **");
    }
}

void test_ads1115_isolated(int addr, const char* name, int freq) {
    Serial.printf("--- %s (0x%02X) @ %d Hz ---\n", name, addr, freq);

    // Disconnect bus first
    Wire.end();
    delay(50);

    Wire.begin(SDA_PIN, SCL_PIN, freq);
    Wire.setTimeOut(50);
    delay(10);

    // Quick ping
    Wire.beginTransmission(addr);
    uint8_t e = Wire.endTransmission();
    Serial.printf("  Ping: %s (err=%d)\n", e == 0 ? "ACK" : "NO ACK", e);

    if (e == 0) {
        // Try reading AIN0
        int16_t raw = 0;
        if (_ads_read(addr, 0, raw)) {
            float v = raw * 0.000125f;
            Serial.printf("  AIN0 raw=%d V=%.3f\n", raw, v);
        }
    }
    Wire.end();
    delay(50);
}

void test_bno085_isolated(int freq) {
    Serial.printf("--- BNO085 (0x%02X) @ %d Hz ---\n", ADDR_BNO, freq);

    Wire.end();
    delay(50);
    Wire.begin(SDA_PIN, SCL_PIN, freq);
    Wire.setTimeOut(50);
    delay(10);

    Wire.beginTransmission(ADDR_BNO);
    uint8_t e = Wire.endTransmission();
    Serial.printf("  Ping: %s (err=%d)\n", e == 0 ? "ACK" : "NO ACK", e);

    Adafruit_BNO08x bno;
    if (e == 0) {
        bool ok = bno.begin_I2C(ADDR_BNO, &Wire);
        Serial.printf("  begin_I2C: %s\n", ok ? "OK" : "FAIL");

        if (ok) {
            bno.enableReport(SH2_GAME_ROTATION_VECTOR, 5000);
            bno.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000);
            bno.enableReport(SH2_ACCELEROMETER, 10000);
            delay(300);

            // Read 5 samples
            int samples = 0;
            uint32_t deadline = millis() + 2000;
            while (samples < 5 && millis() < deadline) {
                sh2_SensorValue_t val;
                if (bno.getSensorEvent(&val)) {
                    if (val.sensorId == SH2_GAME_ROTATION_VECTOR) {
                        float qw = val.un.gameRotationVector.real;
                        float qx = val.un.gameRotationVector.i;
                        float qy = val.un.gameRotationVector.j;
                        float qz = val.un.gameRotationVector.k;
                        Serial.printf("  GRV[%d]: q=[%.3f %.3f %.3f %.3f]\n", samples, qw, qx, qy, qz);
                        samples++;
                    }
                }
                delay(1);
            }
            if (samples == 0) Serial.println("  No GRV data received in 2s");
        }
    }
    Wire.end();
    delay(50);
}

// =============================================================================
// MAIN
// =============================================================================
void setup() {
    delay(2000);
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);

    Serial.println("╔══════════════════════════════════════════════════╗");
    Serial.println("║  EchoGlove V5 — I2C Device Isolation Test v1    ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
    Serial.println();

    // STEP 1: Raw bus lines
    Serial.println("=== STEP 1: Raw bus lines (no I2C init) ===");
    check_bus_lines();
    Serial.println();

    // STEP 2: Scan @ 100kHz
    Serial.println("=== STEP 2: I2C scan @ 100 kHz ===");
    scan_bus(100000, "100kHz");
    Serial.println();

    // STEP 3: Scan @ 400kHz
    Serial.println("=== STEP 3: I2C scan @ 400 kHz ===");
    scan_bus(400000, "400kHz");
    Serial.println();

    // STEP 4-5: ADS1115 #1 isolated @ 100kHz and @ 400kHz
    Serial.println("=== STEP 4: ADS1115 #1 (0x48) @ 100 kHz ===");
    test_ads1115_isolated(ADDR_ADS1, "ADS1115#1", 100000);

    Serial.println("\n=== STEP 5: ADS1115 #1 (0x48) @ 400 kHz ===");
    test_ads1115_isolated(ADDR_ADS1, "ADS1115#1", 400000);

    // STEP 6-7: ADS1115 #2 isolated
    Serial.println("\n=== STEP 6: ADS1115 #2 (0x49) @ 100 kHz ===");
    test_ads1115_isolated(ADDR_ADS2, "ADS1115#2", 100000);

    Serial.println("\n=== STEP 7: ADS1115 #2 (0x49) @ 400 kHz ===");
    test_ads1115_isolated(ADDR_ADS2, "ADS1115#2", 400000);

    // STEP 8-9: BNO085 isolated
    Serial.println("\n=== STEP 8: BNO085 (0x4B) @ 100 kHz ===");
    test_bno085_isolated(100000);

    Serial.println("\n=== STEP 9: BNO085 (0x4B) @ 400 kHz ===");
    test_bno085_isolated(400000);

    // STEP 10: All together @ 100kHz
    Serial.println("\n=== STEP 10: All 3 devices on bus @ 100 kHz ===");
    Wire.end();
    delay(50);
    Wire.begin(SDA_PIN, SCL_PIN, 100000);
    Wire.setTimeOut(50);
    delay(10);

    // Read all flex sensors
    int success = 0;
    for (int ch = 0; ch < 3; ch++) {
        int16_t raw;
        if (_ads_read(ADDR_ADS1, ch, raw)) {
            Serial.printf("  ADS#1 AIN%d raw=%d V=%.3f\n", ch, raw, raw*0.000125f);
            success++;
        }
        delay(20);
    }
    for (int ch = 0; ch < 2; ch++) {
        int16_t raw;
        if (_ads_read(ADDR_ADS2, ch, raw)) {
            Serial.printf("  ADS#2 AIN%d raw=%d V=%.3f\n", ch, raw, raw*0.000125f);
            success++;
        }
        delay(20);
    }
    Serial.printf("  Total successful reads: %d/5\n", success);

    // Quick BNO085 read
    Adafruit_BNO08x bno2;
    if (bno2.begin_I2C(ADDR_BNO, &Wire)) {
        bno2.enableReport(SH2_GAME_ROTATION_VECTOR, 5000);
        delay(200);
        sh2_SensorValue_t val;
        if (bno2.getSensorEvent(&val) && val.sensorId == SH2_GAME_ROTATION_VECTOR) {
            Serial.println("  BNO085 GRV: OK (data received)");
        } else {
            Serial.println("  BNO085 GRV: no data");
        }
    } else {
        Serial.println("  BNO085 begin_I2C: FAILED in multi-device mode");
    }

    // STEP 11: Stability at 100kHz
    Serial.println("\n=== STEP 11: Stability (10 pings to ADS1115#1 @ 100kHz) ===");
    int ping_ok = 0;
    for (int i = 0; i < 10; i++) {
        Wire.beginTransmission(ADDR_ADS1);
        ping_ok += (Wire.endTransmission() == 0);
        delay(100);
    }
    Serial.printf("  %d/10 pings successful\n", ping_ok);
    if (ping_ok == 10) {
        Serial.println("  ✅ Bus stable at 100kHz!");
    }

    Serial.println("\n╔══════════════════════════════════════════════════╗");
    Serial.println("║  ISOLATION TEST COMPLETE. Review above.         ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
}

void loop() {
    delay(10000);
}