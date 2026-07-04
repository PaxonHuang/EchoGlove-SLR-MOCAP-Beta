/* =============================================================================
 * V5 Quick I2C Multi-pin Scan
 * Tests GPIO8/9, GPIO4/5, GPIO1/2 at 100kHz and 400kHz
 * =============================================================================
 */
#include <Arduino.h>
#include <Wire.h>

struct PinPair { int sda, scl; const char* name; };
static const PinPair PAIRS[] = {
    {8, 9, "GPIO8/9 (V5 spec)"},
    {4, 5, "GPIO4/5 (V3 legacy)"},
};
static const int FREQS[] = {100000, 400000};

void scan(int sda, int scl, int freq) {
    Wire.begin(sda, scl, freq);
    Wire.setTimeOut(30);
    int found = 0;
    for (int a = 0x03; a < 0x78; a++) {
        Wire.beginTransmission(a);
        int err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("  0x%02X ", a);
            found++;
        }
    }
    Serial.println();
    Serial.printf("  → %d devices found @ %d kHz\n", found, freq/1000);
    Wire.end();
    delay(50);
}

void setup() {
    delay(1000);
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);
    Serial.println("=== V5 Quick Multi-pin I2C Scan ===\n");

    for (auto& pp : PAIRS) {
        Serial.printf("-- Pin pair: %s --\n", pp.name);
        // Bus line check
        pinMode(pp.sda, INPUT);
        pinMode(pp.scl, INPUT);
        delay(5);
        Serial.printf("  Bus: SDA=%d SCL=%d\n", digitalRead(pp.sda), digitalRead(pp.scl));

        for (int f : FREQS) {
            scan(pp.sda, pp.scl, f);
        }
        Serial.println();
    }

    Serial.println("=== Done. Reset to re-run. ===");
}

void loop() { delay(1000); }