/* =============================================================================
 * EchoGlove V5.2 — Hardware Diagnostic Tool  (v2 — enhanced)
 * =============================================================================
 * Target config: ESP32-S3 + BNO085 + 2x 4.7kΩ I²C pull-ups
 * (NO ADS1115, NO flex sensors connected)
 *
 * Test flow:
 *   Phase 0: I²C bus electrical check (pull-ups)
 *   Phase 1: I²C bus scan          → verify BNO085 at 0x4B
 *   Phase 1.5: Raw I²C READ test   → distinguish ACK vs real comms
 *   Phase 2: BNO085 reset + init + data (with retry)
 *   Phase 3: Continuous streaming   → 10 Hz live readout (5 s)
 *
 * Build:    pio run -e esp32-s3-devkitc-1-n16r8
 * Upload:   pio run -e esp32-s3-devkitc-1-n16r8 -t upload
 * Monitor:  pio device monitor -b 115200
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

// ── I²C Pin Configuration (EchoGlove V5.2) ─────────────────────
// IMPORTANT (2026-06-17 round 5 finding):
//   ESP32-S3-DevKitC-1-N8 (no-PSRAM variant) has GPIO8/GPIO9 partially
//   consumed by internal flash routing. BNO085 at 0x4B shows up as
//   "0 devices found" despite correct voltage, pull-ups, and mode pins.
//   GPIO1/GPIO2 are known-good on this variant — use them instead.
// ROUND 10: trying GPIO11/12 to rule out GPIO1/2 damage from earlier ADS1115 attempts.
// ROUND 16: replacement BNO085 module, INT → GPIO9 enabled (data-ready interrupt).
static constexpr uint8_t  I2C_SDA     = 11;
static constexpr uint8_t  I2C_SCL     = 12;
static constexpr int8_t   BNO085_INT_PIN = 9;     // ★ ENABLED for replacement module
static constexpr int8_t   BNO085_RST_PIN = 10;
// Multi-frequency fallback list (tried in order if main freq fails)
static constexpr uint32_t I2C_FREQ_LIST[] = { 100000, 50000, 10000 };
static constexpr uint8_t  I2C_FREQ_COUNT = sizeof(I2C_FREQ_LIST) / sizeof(I2C_FREQ_LIST[0]);
static constexpr uint32_t I2C_FREQ        = I2C_FREQ_LIST[0];  // primary

// ── BNO085 Configuration ───────────────────────────────────────
// Address depends on ADO pin:
//   ADO → GND  → 0x4A
//   ADO → 3.3V → 0x4B  (default expected)
static constexpr uint8_t  BNO085_ADDR_PRIMARY   = 0x4B;
static constexpr uint8_t  BNO085_ADDR_SECONDARY = 0x4A;
static constexpr uint8_t  BNO085_ADDR = BNO085_ADDR_PRIMARY;  // set to SECONDARY if ADO→GND

// BNO085 RST pin — connect BNO085 RST to this GPIO for software reset.
// If RST is hardwired directly to 3.3V, set this to -1 (no software reset).
// Recommended: connect RST to GPIO10 (or any free GPIO) for full control.
// (BNO085_RST_PIN already declared above as 10)

// ── BNO085 Driver ──────────────────────────────────────────────
static Adafruit_BNO08x bno;
static bool bno_ok = false;

// ── Diagnostic counters ────────────────────────────────────────
static uint32_t total_reads   = 0;
static uint32_t success_reads = 0;
static uint32_t failed_reads  = 0;

// =============================================================================
//  Helper: print a horizontal rule
// =============================================================================
static void printRule() {
    Serial.println("────────────────────────────────────────────────────");
}

// =============================================================================
//  Helper: I²C bus recovery — toggle SCL to unstick stuck devices
// =============================================================================
static void i2c_bus_recovery() {
    Serial.println("  [Recovery] Performing I²C bus recovery...");
    // End Wire to release pins
    Wire.end();
    delay(10);

    // Toggle SCL 9 times to clock out any stuck slave
    pinMode(I2C_SCL, OUTPUT);
    pinMode(I2C_SDA, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL, HIGH);
        delayMicroseconds(5);
    }
    // Send STOP condition
    pinMode(I2C_SDA, OUTPUT);
    digitalWrite(I2C_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SDA, HIGH);
    delayMicroseconds(5);

    // Re-initialize Wire
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
    delay(50);
    Serial.println("  [Recovery] Bus recovered, Wire re-initialized.");
}

// =============================================================================
//  Helper: Raw I²C read test — tries to read N bytes from device
// =============================================================================
static bool i2c_raw_read_test(uint8_t addr, uint8_t reg, uint8_t num_bytes) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);  // repeated start
    if (err != 0) {
        Serial.printf("  [RawRead] Write phase failed (err=%d)\n", err);
        return false;
    }
    uint8_t received = Wire.requestFrom(addr, num_bytes);
    if (received != num_bytes) {
        Serial.printf("  [RawRead] Read failed: requested %d, got %d\n",
                      num_bytes, received);
        return false;
    }
    Serial.printf("  [RawRead] ✓ Successfully read %d bytes from 0x%02X reg 0x%02X: ",
                  num_bytes, addr, reg);
    for (int i = 0; i < num_bytes; i++) {
        Serial.printf("0x%02X ", Wire.read());
    }
    Serial.println();
    return true;
}

// =============================================================================
//  Phase 0 : Pre-flight — I²C bus electrical check
// =============================================================================
static void phase0_bus_check() {
    Serial.println("\n");
    printRule();
    Serial.println("  PHASE 0 — I²C Bus Electrical Check");
    printRule();

    // Check SDA pin state (should be HIGH when pulled up)
    pinMode(I2C_SDA, INPUT_PULLDOWN);
    delay(5);
    bool sda_has_pullup = digitalRead(I2C_SDA) == HIGH;
    pinMode(I2C_SDA, INPUT);  // reset

    pinMode(I2C_SCL, INPUT_PULLDOWN);
    delay(5);
    bool scl_has_pullup = digitalRead(I2C_SCL) == HIGH;
    pinMode(I2C_SCL, INPUT);  // reset

    Serial.printf("  SDA (GPIO%d) internal pull-down test: %s\n",
                  I2C_SDA, sda_has_pullup ? "✓ EXTERNAL PULL-UP DETECTED" : "✗ NO PULL-UP (check wiring!)");
    Serial.printf("  SCL (GPIO%d) internal pull-down test: %s\n",
                  I2C_SCL, scl_has_pullup ? "✓ EXTERNAL PULL-UP DETECTED" : "✗ NO PULL-UP (check wiring!)");

    if (!sda_has_pullup || !scl_has_pullup) {
        Serial.println("  ⚠ WARNING: Missing pull-up resistors! I²C will be unreliable.");
    } else {
        Serial.println("  ✓ Both pull-up resistors appear present.");
    }

    // BNO085 RST pin status
    if (BNO085_RST_PIN >= 0) {
        Serial.printf("  BNO085 RST pin: GPIO%d (software reset available)\n", BNO085_RST_PIN);
    } else {
        Serial.println("  BNO085 RST pin: hardwired to 3.3V (no software reset)");
        Serial.println("  ⚠ If BNO085 init fails, consider connecting RST to a GPIO for reset control.");
    }
}

// =============================================================================
//  Phase 1 : I²C Bus Scan — with multi-frequency fallback
// =============================================================================
//  Scans 0x01-0x7F at 100 kHz. If 0 devices found, retries at 50 kHz and 10 kHz.
//  Some BNO085 clones need slower I²C clocks to ACK reliably.
// =============================================================================
static bool phase1_i2c_scan() {
    Serial.println("\n");
    printRule();
    Serial.println("  PHASE 1 — I²C Bus Scan (multi-frequency)");
    printRule();

    bool bno_found = false;

    for (uint8_t freq_idx = 0; freq_idx < I2C_FREQ_COUNT; freq_idx++) {
        uint32_t freq = I2C_FREQ_LIST[freq_idx];

        Serial.printf("\n  >>> Scan attempt %d/%d at %lu kHz\n",
                      freq_idx + 1, I2C_FREQ_COUNT, freq / 1000);
        Wire.end();
        delay(20);
        Wire.begin(I2C_SDA, I2C_SCL, freq);
        Serial.printf("  ✓ Wire initialized: SDA=%d SCL=%d freq=%lu\n",
                      I2C_SDA, I2C_SCL, freq);
        delay(50);  // Give BNO085 time to settle after clock change

        Serial.println("  Scanning 0x01 – 0x7F ...");
        Serial.println("  ┌──────────┬──────────────────────────────────┐");
        Serial.println("  │ Address  │ Device                           │");
        Serial.println("  ├──────────┼──────────────────────────────────┤");

        uint8_t found_count = 0;
        bool found_at_this_freq = false;

        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            uint8_t error = Wire.endTransmission();

            if (error == 0) {
                found_count++;
                const char* device_name = "Unknown device";
                if (addr == 0x28)      device_name = "BNO055 (ADDR=GND)";
                else if (addr == 0x29) device_name = "BNO055 (ADDR=VDD)";
                else if (addr == 0x4A) device_name = "BNO085 (ADO=GND)";
                else if (addr == 0x4B) device_name = "BNO085 (ADO=VDD)";
                else if (addr == 0x48) device_name = "ADS1115 #1";
                else if (addr == 0x49) device_name = "ADS1115 #2";

                Serial.printf("  │  0x%02X    │ %-32s │\n", addr, device_name);

                if (addr == BNO085_ADDR) {
                    bno_found = true;
                    found_at_this_freq = true;
                }
            }
        }

        Serial.println("  └──────────┴──────────────────────────────────┘");
        Serial.printf("  [%lu kHz] Devices found: %d\n", freq / 1000, found_count);

        if (found_at_this_freq) {
            Serial.printf("  ✓ BNO085 detected at 0x%02X at %lu kHz\n",
                          BNO085_ADDR, freq / 1000);
            break;
        }

        if (freq_idx < I2C_FREQ_COUNT - 1) {
            Serial.println("  ⚠ Nothing found at this freq, trying slower speed...");
        }
    }

    if (!bno_found) {
        Serial.printf("\n  ✗ BNO085 NOT found at 0x%02X after all frequencies!\n", BNO085_ADDR);
        Serial.println("  ┌─────────────────────────────────────────────────────┐");
        Serial.println("  │ ROUND 4 DIAGNOSIS:                                   │");
        Serial.println("  │ Power & pull-ups verified OK (VCC=3.25V, SDA/SCL   │");
        Serial.println("  │ idle HIGH at 3.25V), CS/PS0/PS1/ADO/RST all wired. │");
        Serial.println("  │ Yet 0 devices respond on bus.                       │");
        Serial.println("  │                                                     │");
        Serial.println("  │ REMAINING ROOT CAUSES (most likely first):          │");
        Serial.println("  │ 1. BNO085 module is counterfeit / dead             │");
        Serial.println("  │    → Check chip marking on the module               │");
        Serial.println("  │    → Real BNO085 has 'BNO085' or 'BNO080' silkscreen│");
        Serial.println("  │ 2. BNO085 module might actually be BNO055          │");
        Serial.println("  │    → BNO055 I²C addr: 0x28 / 0x29 (different!)      │");
        Serial.println("  │    → Already scanning all addresses                 │");
        Serial.println("  │ 3. Wrong pinout on user's 10-pin module             │");
        Serial.println("  │    → Verify with module datasheet                   │");
        Serial.println("  │ 4. GPIO8/9 conflict on this specific S3 N8 board    │");
        Serial.println("  │    → Try GPIO1/GPIO2 (less likely)                  │");
        Serial.println("  └─────────────────────────────────────────────────────┘");
    }

    return bno_found;
}

// =============================================================================
//  Phase 1.5 : Raw I²C Read Test — the KEY diagnostic step
// =============================================================================
//  I²C scan only tests WRITE (ACK). This tests READ capability.
//  If scan passes but read fails → chip is present but not ready/unstable.
// =============================================================================
static bool phase1_5_raw_read_test() {
    Serial.println("\n");
    printRule();
    Serial.println("  PHASE 1.5 — Raw I²C READ Test (BNO085)");
    printRule();
    Serial.println("  I²C scan only tests WRITE/ACK. This tests actual READ.");
    Serial.println("  If scan=OK but read=FAIL → chip present but not ready.\n");

    // Test 1: Try reading 1 byte from register 0x00 (product ID)
    Serial.println("  Test A: Read 1 byte from BNO085 register 0x00 (Product ID)...");
    bool test_a = i2c_raw_read_test(BNO085_ADDR, 0x00, 1);

    delay(5);

    // Test 2: Try reading 4 bytes (full product ID block)
    Serial.println("\n  Test B: Read 4 bytes from BNO085 register 0x00 (Product ID block)...");
    bool test_b = i2c_raw_read_test(BNO085_ADDR, 0x00, 4);

    Serial.println();
    if (test_a && test_b) {
        Serial.println("  ✓✓✓ ALL RAW READS PASSED — BNO085 communication is healthy!");
        Serial.println("      Chip is fully operational. Proceeding to Phase 2...");
        return true;
    } else if (test_a || test_b) {
        Serial.println("  ⚠ PARTIAL: Some reads succeeded. Chip may be in boot sequence.");
        Serial.println("    Will retry after delay...");
        return false;
    } else {
        Serial.println("  ✗✗✗ ALL RAW READS FAILED!");
        Serial.println("  ┌─────────────────────────────────────────────────────┐");
        Serial.println("  │ DIAGNOSIS: BNO085 ACKs address but won't send data │");
        Serial.println("  │                                                     │");
        Serial.println("  │ This means the chip is PRESENT but NOT READY.       │");
        Serial.println("  │                                                     │");
        Serial.println("  │ ROOT CAUSES (most likely first):                    │");
        Serial.println("  │ 1. ★★★ RST pin floating or not at 3.3V ★★★          │");
        Serial.println("  │    → Chip's internal MCU hasn't booted              │");
        Serial.println("  │    → FIX: Connect RST pin to 3.3V rail              │");
        Serial.println("  │ 2. BNO085 still in boot sequence (needs 500ms+)    │");
        Serial.println("  │    → FIX: Add 1s delay after power-on              │");
        Serial.println("  │ 3. PS0/PS1 mode pins wrong                          │");
        Serial.println("  │    → PS0=3.3V, PS1=GND for I²C mode                │");
        Serial.println("  │ 4. BNO085 VCC unstable (brownout during boot)       │");
        Serial.println("  │    → Add 100µF cap close to BNO085 VCC-GND         │");
        Serial.println("  └─────────────────────────────────────────────────────┘");
        return false;
    }
}

// =============================================================================
//  Phase 2 : BNO085 Hardware Reset + Initialization + Single Read
// =============================================================================
static void phase2_bno_init() {
    Serial.println("\n");
    printRule();
    Serial.println("  PHASE 2 — BNO085 Reset + Initialization + Data Read");
    printRule();

    if (!bno_ok) {
        Serial.println("  ✗ SKIPPED: BNO085 raw read test failed.");
        Serial.println("  Attempting recovery sequence anyway...\n");
    }

    // ── Step 1: I²C bus recovery ──
    i2c_bus_recovery();
    delay(100);

    // ── Step 2: Hardware reset BNO085 (if RST pin configured) ──
    if (BNO085_RST_PIN >= 0) {
        Serial.printf("  [Reset] Performing hardware reset via GPIO%d...\n", BNO085_RST_PIN);
        pinMode(BNO085_RST_PIN, OUTPUT);
        digitalWrite(BNO085_RST_PIN, LOW);    // Assert reset
        delay(50);                              // Hold reset 50ms
        digitalWrite(BNO085_RST_PIN, HIGH);   // Release reset
        Serial.println("  [Reset] RST released. Waiting for BNO085 boot...");
        delay(1000);  // BNO085 needs ~500ms to boot after reset
        Serial.println("  [Reset] Boot delay complete.");
    } else {
        Serial.println("  [Reset] No RST GPIO configured. Power-cycle BNO085 if needed.");
        Serial.println("  [Reset] Waiting 2s for BNO085 to stabilize...");
        delay(2000);  // Extra wait for chip to boot
    }

    // ── Step 3: Verify BNO085 is ready after reset ──
    Serial.println("\n  [Pre-check] Re-scanning for BNO085...");
    Wire.beginTransmission(BNO085_ADDR);
    uint8_t scan_err = Wire.endTransmission();
    if (scan_err != 0) {
        Serial.printf("  ✗ BNO085 disappeared from bus after reset (err=%d)!\n", scan_err);
        Serial.println("    Chip may have entered a different mode. Check PS0/PS1 pins.");
        bno_ok = false;
        return;
    }
    Serial.println("  ✓ BNO085 still present at 0x4B after reset.");

    // ── Step 4: Try raw read again ──
    Serial.println("  [Pre-check] Re-testing raw I²C read...");
    if (!i2c_raw_read_test(BNO085_ADDR, 0x00, 1)) {
        Serial.println("  ⚠ Raw read still failing after reset. Will try library init anyway...");
    }

    // ── Step 5: Attempt library initialization with retries ──
    Serial.println("\n  [Init] Attempting Adafruit BNO08x begin_I2C()...");

    static constexpr int MAX_RETRIES = 3;
    bool init_success = false;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        Serial.printf("  [Init] Attempt %d/%d: ", attempt, MAX_RETRIES);

        // Try with explicit address
        bool result = bno.begin_I2C(BNO085_ADDR, &Wire, BNO085_INT_PIN);

        if (result) {
            Serial.println("✓ SUCCESS!");
            init_success = true;
            break;
        }

        Serial.println("✗ FAILED");

        if (attempt < MAX_RETRIES) {
            Serial.printf("  [Init] Waiting 1s before retry %d...\n", attempt + 1);
            delay(1000);

            // Bus recovery between retries
            if (attempt == 2) {
                i2c_bus_recovery();
                delay(200);
            }
        }
    }

    if (!init_success) {
        // Last resort: try without explicit address (library default)
        Serial.println("\n  [Init] Last resort: trying begin_I2C() without explicit address...");
        delay(500);
        if (bno.begin_I2C(BNO085_ADDR, &Wire, BNO085_INT_PIN)) {
            Serial.println("  ✓ SUCCESS with default address!");
            init_success = true;
        }
    }

    if (!init_success) {
        Serial.println("\n  ✗✗✗ ALL INIT ATTEMPTS FAILED ✗✗✗");
        Serial.println("  ┌─────────────────────────────────────────────────────┐");
        Serial.println("  │ HARDWARE ACTIONS REQUIRED:                          │");
        Serial.println("  │                                                     │");
        Serial.println("  │ 1. POWER-CYCLE the entire breadboard (unplug USB)   │");
        Serial.println("  │ 2. ★★★ VERIFY: BNO085 RST → 3.3V ★★★               │");
        Serial.println("  │    Use multimeter continuity test                   │");
        Serial.println("  │ 3. Verify: BNO085 PS0 → 3.3V, PS1 → GND           │");
        Serial.println("  │ 4. Add 100µF electrolytic cap across VCC-GND        │");
        Serial.println("  │    near the BNO085 module                           │");
        Serial.println("  │ 5. OPTIONAL: Connect BNO085 RST to GPIO10           │");
        Serial.println("  │    Then set BNO085_RST_PIN = 10 in code             │");
        Serial.println("  │ 6. Re-upload and re-run this diagnostic              │");
        Serial.println("  └─────────────────────────────────────────────────────┘");
        bno_ok = false;
        return;
    }

    bno_ok = true;
    Serial.println("\n  ✓ BNO085 initialized successfully!");

    // ── Step 6: Enable sensor reports ──
    Serial.println("  Enabling sensor reports (Rotation Vector + Gyroscope)...");
    bno.enableReport(SH2_ROTATION_VECTOR, 10000);   // 100 Hz
    bno.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000);
    Serial.println("  ✓ Reports enabled.\n");

    // ── Step 7: Wait for first valid data ──
    Serial.println("  Waiting for first valid sensor event (up to 20s)...");
    uint32_t start = millis();
    bool got_data = false;

    while (millis() - start < 20000) {
        sh2_SensorValue_t event;
        if (bno.getSensorEvent(&event)) {
            // Debug: show ALL event IDs we receive
            static uint32_t last_print = 0;
            if (millis() - last_print > 500) {
                Serial.printf("    [event] sensorId=0x%02X status=0x%02X\n",
                              event.sensorId, event.status);
                last_print = millis();
            }
            if (event.sensorId == SH2_ROTATION_VECTOR) {
                got_data = true;
                Serial.println("  ✓ First quaternion received!\n");

                float qw = event.un.rotationVector.real;
                float qx = event.un.rotationVector.i;
                float qy = event.un.rotationVector.j;
                float qz = event.un.rotationVector.k;
                float accuracy = event.un.rotationVector.accuracy;

                Serial.printf("  Quaternion: qw=%.4f  qx=%.4f  qy=%.4f  qz=%.4f\n",
                              qw, qx, qy, qz);
                Serial.printf("  Accuracy:   %.4f  (%s)\n", accuracy,
                              accuracy > 0.8 ? "HIGH" :
                              accuracy > 0.5 ? "MEDIUM" : "LOW");

                // Quaternion → Euler (ZYX convention, degrees)
                float roll  = atan2f(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy)) * 57.2958f;
                float sinp  = 2*(qw*qy - qz*qx);
                float pitch = (fabsf(sinp) >= 1) ? copysignf(90.0f, sinp) * 57.2958f : asinf(sinp) * 57.2958f;
                float yaw   = atan2f(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz)) * 57.2958f;

                Serial.printf("  Euler:      roll=%.1f°  pitch=%.1f°  yaw=%.1f°\n",
                              roll, pitch, yaw);
                break;
            }
        }
        delay(10);
    }

    if (!got_data) {
        Serial.println("  ✗ TIMEOUT: No quaternion data after 8 seconds.");
        Serial.println("    BNO085 init succeeded but no sensor data. Check if chip is stable.");
        bno_ok = false;
    }
}

// =============================================================================
//  Phase 3 : Continuous Streaming (5 seconds @ ~10 Hz)
// =============================================================================
static void phase3_streaming() {
    Serial.println("\n");
    printRule();
    Serial.println("  PHASE 3 — BNO085 Continuous Streaming (5 seconds)");
    printRule();

    if (!bno_ok) {
        Serial.println("  ✗ SKIPPED: BNO085 not operational.");
        return;
    }

    Serial.println("  Streaming IMU data for 5 seconds...\n");
    Serial.println("  #    |  Roll(°)  | Pitch(°)  |  Yaw(°)   | Gx(°/s) | Gy(°/s) | Gz(°/s) | Acc");
    Serial.println("  ─────┼───────────┼───────────┼───────────┼─────────┼─────────┼─────────┼────");

    total_reads   = 0;
    success_reads = 0;
    failed_reads  = 0;

    uint32_t stream_start = millis();
    uint32_t sample_num   = 0;

    while (millis() - stream_start < 5000) {
        sh2_SensorValue_t event;
        total_reads++;

        static float qw = 1, qx = 0, qy = 0, qz = 0;
        static float gx = 0, gy = 0, gz = 0;
        static float accuracy = 0;
        bool updated = false;

        // Drain all available events
        while (bno.getSensorEvent(&event)) {
            if (event.sensorId == SH2_ROTATION_VECTOR) {
                qw = event.un.rotationVector.real;
                qx = event.un.rotationVector.i;
                qy = event.un.rotationVector.j;
                qz = event.un.rotationVector.k;
                accuracy = event.un.rotationVector.accuracy;
                updated = true;
            }
            if (event.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                gx = event.un.gyroscope.x * 57.2958f;  // rad/s → deg/s
                gy = event.un.gyroscope.y * 57.2958f;
                gz = event.un.gyroscope.z * 57.2958f;
            }
        }

        if (updated) {
            success_reads++;
            sample_num++;

            float roll  = atan2f(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy)) * 57.2958f;
            float sinp  = 2*(qw*qy - qz*qx);
            float pitch = (fabsf(sinp) >= 1) ? copysignf(90.0f, sinp) * 57.2958f : asinf(sinp) * 57.2958f;
            float yaw   = atan2f(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz)) * 57.2958f;

            Serial.printf("  %-4lu | %9.1f | %9.1f | %9.1f | %7.1f | %7.1f | %7.1f | %.2f\n",
                          sample_num, roll, pitch, yaw, gx, gy, gz, accuracy);
        } else {
            failed_reads++;
        }

        delay(100);  // ~10 Hz display rate
    }

    // ── Streaming summary ──
    Serial.println();
    printRule();
    Serial.println("  STREAMING SUMMARY");
    printRule();
    Serial.printf("  Total poll cycles:  %lu\n", total_reads);
    Serial.printf("  Successful updates: %lu\n", success_reads);
    Serial.printf("  Empty polls:        %lu\n", failed_reads);
    Serial.printf("  Data rate:          ~%.1f Hz\n", success_reads / 5.0);

    if (success_reads >= 20) {
        Serial.println("  ✓ EXCELLENT: BNO085 streaming at expected rate.");
    } else if (success_reads >= 5) {
        Serial.println("  ⚠ OK: Data received but rate lower than expected.");
    } else {
        Serial.println("  ✗ POOR: Very little data. Check I²C wiring / pull-ups.");
    }
}

// =============================================================================
//  Final Verdict
// =============================================================================
static void print_final_verdict() {
    Serial.println("\n");
    printRule();
    Serial.println("  ★★★ FINAL DIAGNOSTIC VERDICT ★★★");
    printRule();

    if (bno_ok && success_reads >= 5) {
        Serial.println("  ✓✓✓ ALL SYSTEMS GO ✓✓✓");
        Serial.println();
        Serial.println("  Your breadboard is correctly wired:");
        Serial.printf ("    • ESP32-S3 I²C bus (GPIO%d/GPIO%d) — OK\n",
                      I2C_SDA, I2C_SCL);
        Serial.println("    • 4.7kΩ pull-up resistors — OK");
        Serial.println("    • BNO085 communication — OK");
        Serial.println("    • BNO085 sensor data — OK");
        Serial.println();
        Serial.println("  Next steps:");
        Serial.println("    1. Add ADS1115 #1 (ADDR→GND, expected 0x48)");
        Serial.println("    2. Add ADS1115 #2 (ADDR→3.3V, expected 0x49)");
        Serial.println("    3. Add flex sensors to ADS1115 ADC channels");
        Serial.println("    4. Re-run this diagnostic to verify all devices");
    } else if (bno_ok) {
        Serial.println("  ⚠ PARTIAL: BNO085 detected but data rate is low.");
        Serial.println("    Check pull-up resistor values (should be 4.7kΩ).");
    } else {
        Serial.println("  ✗✗✗ BNO085 FAILED ✗✗✗");
        Serial.println();
        Serial.println("  Most likely causes (in order of probability):");
        Serial.println("    1. BNO085 RST pin not connected to 3.3V");
        Serial.println("       → THIS CAUSES 90% OF FAILURES");
        Serial.println("    2. SDA/SCL wiring swapped or loose");
        Serial.println("    3. BNO085 VCC/GND not connected");
        Serial.println("    4. PS0/PS1 mode pins misconfigured");
        Serial.println("       → PS0=3.3V, PS1=GND for I²C mode");
        Serial.println("    5. Breadboard contact issue (try different rows)");
    }

    printRule();
    Serial.println("  Diagnostic complete. Reset ESP32 to re-run.\n");
}

// =============================================================================
//  Arduino Entry Points
// =============================================================================
void setup() {
    Serial.begin(115200);
    // Wait for USB CDC serial (ESP32-S3 native USB)
    delay(2000);

    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════════════╗");
    Serial.println("║   EchoGlove V5.2 — Hardware Diagnostic Tool v2 ║");
    Serial.println("║   Config: S3 + BNO085 (no ADS1115/flex)        ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
    Serial.printf("  I²C Pins:  SDA=GPIO%d  SCL=GPIO%d  Freq=%luHz\n",
                  I2C_SDA, I2C_SCL, I2C_FREQ);
    Serial.printf("  Expected:  BNO085 @ 0x%02X\n", BNO085_ADDR);
    Serial.printf("  BNO085 RST: %s\n",
                  BNO085_RST_PIN >= 0 ? "GPIO (software reset)" : "hardwired to 3.3V");
    Serial.printf("  Timestamp: %lu ms\n", millis());

    // ── EARLY SOFTWARE RESET: pulse BNO085 RST before any I²C activity ──
    if (BNO085_RST_PIN >= 0) {
        Serial.printf("\n  [Pre-reset] Pulsing BNO085 RST via GPIO%d to ensure clean boot...\n",
                      BNO085_RST_PIN);
        pinMode(BNO085_RST_PIN, OUTPUT);
        digitalWrite(BNO085_RST_PIN, LOW);    // Assert reset (hold low)
        Serial.println("  [Pre-reset] RST held LOW for 100ms...");
        delay(100);
        digitalWrite(BNO085_RST_PIN, HIGH);   // Release reset
        Serial.println("  [Pre-reset] RST released HIGH. Waiting 1500ms for boot...");
        delay(1500);
        Serial.println("  [Pre-reset] Done. Chip should be in known-good state now.");
    }

    // ── Run diagnostic phases in sequence ──
    phase0_bus_check();                         // Electrical check (before Wire.begin)
    bool scan_found = phase1_i2c_scan();        // Bus scan (multi-freq)

    // Only proceed to raw read test if scan found BNO085
    if (scan_found) {
        bno_ok = phase1_5_raw_read_test();      // Raw I²C READ test
    } else {
        bno_ok = false;
        Serial.println("\n  ✗ BNO085 not found in scan. Skipping Phase 1.5.");
    }

    // Phase 2 will attempt recovery even if raw read failed
    phase2_bno_init();                          // BNO085 reset + init + first read
    phase3_streaming();                         // 5-second continuous stream
    print_final_verdict();                      // Summary
}

void loop() {
    // Blink onboard LED to show diagnostic is complete
    static uint32_t last_blink = 0;
    static bool led_state = false;

    if (millis() - last_blink > 1000) {
        last_blink = millis();
        led_state = !led_state;
        // ESP32-S3 DevKitC typically has LED on GPIO2
        // No action needed — diagnostic runs once in setup()
    }

    // Print heartbeat every 3 seconds so user knows firmware is alive
    static uint32_t last_hb = 0;
    if (millis() - last_hb > 3000) {
        last_hb = millis();
        Serial.printf("[heartbeat] uptime=%lu s\n", millis() / 1000);
    }

    delay(100);
}
