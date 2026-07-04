/**
 * I2C Bus Recovery + Multi-pin Scan Diagnostic v2
 *
 * Phase 0: System info (chip/flash/psram)
 * Phase 1: Direct GPIO read on candidate pin pairs (no I2C, just digitalRead)
 * Phase 2: Bus recovery — send 9 SCL clock pulses to release stuck SDA
 * Phase 3: Wire.h scan at 100 kHz on each pin pair with progress every 16 addrs
 * Phase 4: Wire.h scan at 10 kHz (slow fallback) on best pin pair
 * Phase 5: Targeted probe of 0x48, 0x49, 0x4A, 0x4B with verbose status
 */
#include <Arduino.h>
#include <Wire.h>

// Pin pairs to test
static const uint8_t PIN_PAIRS[][2] = {
    {8, 9},       // Primary (CLAUDE.md / SOP spec)
    {4, 5},       // Alt per old data_structures.h
    {1, 2},       // Common alt
};
static const size_t NUM_PAIRS = sizeof(PIN_PAIRS) / sizeof(PIN_PAIRS[0]);

// Known device addresses
static const uint8_t ADDR_ADS1115_1  = 0x48;
static const uint8_t ADDR_ADS1115_2  = 0x49;
static const uint8_t ADDR_BNO085_ALT = 0x4A;
static const uint8_t ADDR_BNO085     = 0x4B;

const char* deviceLabel(uint8_t addr) {
    switch (addr) {
        case ADDR_ADS1115_1:  return "ADS1115 #1";
        case ADDR_ADS1115_2:  return "ADS1115 #2";
        case ADDR_BNO085:     return "BNO085";
        case ADDR_BNO085_ALT: return "BNO085 (alt)";
        default:              return nullptr;
    }
}

void printPhaseHeader(const char* title) {
    Serial.println();
    Serial.println("========================================");
    Serial.print("  ");
    Serial.println(title);
    Serial.println("========================================");
}

void printPinStatus(int sda, int scl) {
    // Configure as input and read (need to disable internal pull-up to see real level)
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delay(5);
    int sda_lvl = digitalRead(sda);
    int scl_lvl = digitalRead(scl);
    Serial.printf("    SDA=GPIO%d=%d  SCL=GPIO%d=%d\n", sda, sda_lvl, scl, scl_lvl);
}

void bus_recovery(int sda, int scl) {
    Serial.printf("\n--- Bus Recovery (SDA=%d, SCL=%d) ---\n", sda, scl);

    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delay(10);
    Serial.printf("  Before recovery: SDA=%d, SCL=%d\n", digitalRead(sda), digitalRead(scl));

    if (digitalRead(sda) == HIGH && digitalRead(scl) == HIGH) {
        Serial.println("  Bus not stuck — recovery not needed.");
        return;
    }

    Serial.println("  Bus stuck! Performing 9-clock recovery...");

    pinMode(scl, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        pinMode(sda, INPUT);
        if (digitalRead(sda) == HIGH) {
            Serial.printf("  SDA released after %d clock pulses!\n", i + 1);
            break;
        }
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
    }

    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);

    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delay(10);
    Serial.printf("  After recovery: SDA=%d, SCL=%d\n", digitalRead(sda), digitalRead(scl));

    if (digitalRead(sda) == HIGH && digitalRead(scl) == HIGH) {
        Serial.println("  Bus recovery SUCCESS — SDA and SCL both HIGH");
    } else {
        Serial.println("  Bus recovery FAILED — SDA still LOW. Hardware issue?");
    }
}

struct ScanResult {
    uint8_t sda, scl;
    uint32_t freq;
    uint8_t device_count;
    uint8_t found_addrs[16];
    int err_counts[8];
    uint32_t duration_ms;
};

void i2c_scan(int sda, int scl, uint32_t freq, ScanResult& result) {
    Serial.printf("\n--- I2C Scan: SDA=%d SCL=%d freq=%u Hz ---\n", sda, scl, freq);

    Wire.begin(sda, scl, freq);
    Wire.setTimeOut(50);  // 50ms

    result = {sda, scl, freq, 0, {}, {0,0,0,0,0,0,0,0}, 0};
    uint32_t t0 = millis();

    Serial.printf("  Scanning 0x03..0x77 (%u addresses)...\n", 0x77 - 0x03 + 1);
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err < 8) result.err_counts[err]++;

        if (err == 0) {
            if (result.device_count < 16)
                result.found_addrs[result.device_count] = addr;
            result.device_count++;
            const char* lbl = deviceLabel(addr);
            Serial.printf("    [0x%02X] FOUND %s\n", addr, lbl ? lbl : "");
        } else if (err == 5) {
            // timeout — likely stuck bus, abort early to save time
            Serial.printf("    [0x%02X] TIMEOUT (err=5) — bus stuck, aborting scan\n", addr);
            break;
        }
        if ((addr & 0x0F) == 0x0F) {
            Serial.printf("    ...progress 0x%02X\n", addr);
        }
    }
    result.duration_ms = millis() - t0;

    Serial.printf("  Total: %d devices found in %u ms\n", result.device_count, result.duration_ms);
    Serial.printf("  Error histogram: err0=%d err1=%d err2=%d err3=%d err4=%d err5=%d err6=%d err7=%d\n",
                  result.err_counts[0], result.err_counts[1], result.err_counts[2], result.err_counts[3],
                  result.err_counts[4], result.err_counts[5], result.err_counts[6], result.err_counts[7]);

    Wire.end();
    delay(50);
}

void bno085_targeted_probe(int sda, int scl, uint32_t freq) {
    Serial.printf("\n--- Targeted BNO085/ADS1115 probe (SDA=%d SCL=%d freq=%u Hz) ---\n", sda, scl, freq);

    Wire.begin(sda, scl, freq);
    Wire.setTimeOut(50);

    // Check pin state after Wire.begin
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delay(5);
    Serial.printf("  Pin state: SDA=%d SCL=%d\n", digitalRead(sda), digitalRead(scl));

    const uint8_t addrs[] = {0x48, 0x49, 0x4A, 0x4B};
    for (size_t i = 0; i < sizeof(addrs); i++) {
        uint8_t a = addrs[i];
        Serial.printf("  Probing 0x%02X ... ", a);
        Wire.beginTransmission(a);
        uint8_t err = Wire.endTransmission();
        Serial.printf("err=%d %s\n", err, err == 0 ? "(ACK)" : "(no ACK)");
    }

    Wire.end();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(1500);

    printPhaseHeader("EchoGlove V5 — I2C Hardware Diagnostic v2");
    Serial.printf("  Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("  Flash: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("  PSRAM: %u bytes (%u MB)\n",
                  ESP.getPsramSize(), ESP.getPsramSize() / (1024 * 1024));
    if (ESP.getPsramSize() == 0) {
        Serial.println("  ** WARNING: PSRAM=0 → memory_type mismatch in board config **");
    }

    // Phase 1: Direct GPIO read on each candidate pin pair
    printPhaseHeader("Phase 1: Direct GPIO Read (no I2C, no pull-up config)");
    for (size_t i = 0; i < NUM_PAIRS; i++) {
        Serial.printf("  Pin pair #%d: SDA=%d, SCL=%d\n", i, PIN_PAIRS[i][0], PIN_PAIRS[i][1]);
        printPinStatus(PIN_PAIRS[i][0], PIN_PAIRS[i][1]);
        delay(50);
    }

    // Phase 2: Bus recovery on each pin pair
    printPhaseHeader("Phase 2: I2C Bus Recovery");
    for (size_t i = 0; i < NUM_PAIRS; i++) {
        bus_recovery(PIN_PAIRS[i][0], PIN_PAIRS[i][1]);
        delay(100);
    }

    // Phase 3: Wire.h scan at 100 kHz on each pin pair
    printPhaseHeader("Phase 3: Wire.h I2C Scan @ 100 kHz");
    ScanResult bestResult = {};
    bool anySuccess = false;
    for (size_t i = 0; i < NUM_PAIRS; i++) {
        ScanResult r;
        i2c_scan(PIN_PAIRS[i][0], PIN_PAIRS[i][1], 100000, r);
        if (r.device_count > bestResult.device_count) {
            bestResult = r;
            anySuccess = true;
        }
        delay(100);
    }

    if (anySuccess) {
        Serial.printf("\n  BEST pin pair: SDA=%d SCL=%d (%d devices)\n",
                      bestResult.sda, bestResult.scl, bestResult.device_count);
    } else {
        Serial.println("\n  No devices found on any pin pair @ 100 kHz");
    }

    // Phase 4: Targeted probe of expected addresses
    printPhaseHeader("Phase 4: Targeted Probe @ 100 kHz");
    for (size_t i = 0; i < NUM_PAIRS; i++) {
        bno085_targeted_probe(PIN_PAIRS[i][0], PIN_PAIRS[i][1], 100000);
        delay(100);
    }

    // Phase 5: Retry at 10 kHz on primary pins (slower = more robust)
    printPhaseHeader("Phase 5: Retry @ 10 kHz (slow) on GPIO8/9");
    ScanResult r;
    i2c_scan(8, 9, 10000, r);

    printPhaseHeader("Diagnostic Complete");
    Serial.println("  Halting (loop is idle).");
}

void loop() {
    delay(10000);
}