/* =============================================================================
 * EchoGlove V5 — Deep Hardware Diagnostic
 * =============================================================================
 * Tests: I2C bus, BNO085 (0x4B), ADS1115 (0x48, 0x49)
 * Requires: ESP32-S3 + BNO085 + pull-up resistors on SDA/SCL
 * ============================================================================= */
#include <Arduino.h>
#include <Wire.h>

// I2C pins
#define SDA_PIN 8
#define SCL_PIN 9
#define I2C_FREQ 100000

// Expected addresses
#define ADDR_ADS1_1  0x48
#define ADDR_ADS1_2  0x49
#define ADDR_BNO085  0x4B

void scanI2CBus() {
    Serial.println("\n--- I2C Bus Scan (0x03..0x77) ---");
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("  [OK] 0x%02X", addr);
            if (addr == ADDR_ADS1_1) Serial.print(" (ADS1115 #1)");
            else if (addr == ADDR_ADS1_2) Serial.print(" (ADS1115 #2)");
            else if (addr == ADDR_BNO085) Serial.print(" (BNO085)");
            Serial.println();
            found++;
        }
    }
    Serial.printf("Scan complete: %d device(s) found\n", found);
}

void probeBNO085() {
    Serial.println("\n--- BNO085 Probe (0x4B) ---");

    // Try basic I2C communication
    Wire.beginTransmission(ADDR_BNO085);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
        Serial.println("  BNO085 responded on I2C! Device present.");
    } else {
        Serial.printf("  BNO085 NOT responding. err=%d\n", err);
        Serial.println("  Possible causes:");
        Serial.println("    1. PS0 pin not connected to GND (must be LOW for I2C)");
        Serial.println("    2. ADO pin not connected to 3.3V (must be HIGH for 0x4B)");
        Serial.println("    3. RST pin floating or connected wrong");
        Serial.println("    4. Missing pull-up resistors on SDA/SCL");
        Serial.println("    5. Wiring error (SDA/SCL swapped, loose connection)");
    }
}

void probeADS1115(uint8_t addr, const char* name) {
    Serial.printf("\n--- %s Probe (0x%02X) ---\n", name, addr);
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
        Serial.printf("  %s responded! Device present.\n", name);
    } else {
        Serial.printf("  %s NOT responding. err=%d (expected if not connected)\n", name, err);
    }
}

void testGPIO() {
    Serial.println("\n--- GPIO State Check ---");
    // Check SDA/SCL state without I2C
    pinMode(SDA_PIN, INPUT);
    pinMode(SCL_PIN, INPUT);
    Serial.printf("  GPIO%d (SDA) raw: %d\n", SDA_PIN, digitalRead(SDA_PIN));
    Serial.printf("  GPIO%d (SCL) raw: %d\n", SCL_PIN, digitalRead(SCL_PIN));

    // With internal pull-up
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delay(10);
    Serial.printf("  GPIO%d (SDA) pull-up: %d\n", SDA_PIN, digitalRead(SDA_PIN));
    Serial.printf("  GPIO%d (SCL) pull-up: %d\n", SCL_PIN, digitalRead(SCL_PIN));

    // Should be HIGH when pull-ups present
    if (digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH) {
        Serial.println("  PASS: Both lines HIGH (pull-ups working)");
    } else {
        Serial.println("  WARN: Lines not both HIGH — check pull-ups or wiring");
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);  // Wait for USB CDC to be ready

    Serial.println("\n\n========================================");
    Serial.println("  EchoGlove V5 — Deep Hardware Diagnostic");
    Serial.println("========================================\n");

    Serial.printf("ESP32-S3 Chip: %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    if (psramFound()) {
        Serial.printf("PSRAM: %d KB\n", ESP.getPsramSize() / 1024);
    } else {
        Serial.println("PSRAM: Not found");
    }

    // Step 1: GPIO check
    testGPIO();

    // Step 2: Init I2C
    Serial.println("\n--- I2C Bus Init ---");
    Serial.printf("  SDA=GPIO%d, SCL=GPIO%d, Freq=%dkHz\n", SDA_PIN, SCL_PIN, I2C_FREQ / 1000);
    Wire.begin(SDA_PIN, SCL_PIN, I2C_FREQ);
    Wire.setTimeOut(100);  // 100ms timeout
    Serial.println("  I2C bus initialized.");

    // Step 3: Scan
    scanI2CBus();

    // Step 4: Probe specific devices
    probeBNO085();
    probeADS1115(ADDR_ADS1_1, "ADS1115 #1");
    probeADS1115(ADDR_ADS1_2, "ADS1115 #2");

    Serial.println("\n========================================");
    Serial.println("  Diagnostic Complete");
    Serial.println("========================================");
    Serial.println("\nContinuing to loop (tick every 2s)...");
}

void loop() {
    static uint32_t tick = 0;
    Serial.printf("tick %lu\n", ++tick);
    delay(2000);
}
