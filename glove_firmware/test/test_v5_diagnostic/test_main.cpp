/* =============================================================================
 * EchoGlove V5 — Comprehensive Hardware Diagnostic v3
 * =============================================================================
 *
 * Tests ALL V5 hardware components in sequence with clear PASS/FAIL output.
 * Designed to run as a standalone Arduino sketch on ESP32-S3-N16R8.
 *
 * Test sequence (8 phases):
 *   Phase 0: System Info (chip model, flash, PSRAM)
 *   Phase 1: I2C Bus Scan (full 0x03–0x77, verify all 3 devices)
 *   Phase 2: ADS1115 #1 (0x48, ADDR→GND) — Flex 0-2 (Thumb/Index/Middle)
 *   Phase 3: ADS1115 #2 (0x49, ADDR→VDD) — Flex 3-4 (Ring/Pinky)
 *   Phase 4: BNO085 Init (0x4B, Game Rotation Vector + Accel + Gyro)
 *   Phase 5: BNO085 Data Stream (quaternion, euler, gyro, accel)
 *   Phase 6: Flex Sensor Responsiveness (bend each finger interactively)
 *   Phase 7: Full Feature Array (11-dim: flex[5] + euler[3] + gyro[3])
 *   Phase 8: Stability & Noise (50 I2C pings, 100-sample noise check)
 *
 * Wiring (V5 DualGloveFlex):
 *   I2C: SDA=GPIO8, SCL=GPIO9, 4.7kΩ pull-ups to 3.3V
 *   BNO085 (GY-BNO085): VCC→3.3V, PS0→GND, PS1→GND, ADO→3.3V (0x4B)
 *   ADS1115 #1: ADDR→GND (0x48) — Flex 0=Thumb, 1=Index, 2=Middle
 *   ADS1115 #2: ADDR→VDD (0x49) — Flex 3=Ring, 4=Pinky
 *   5x Flex sensors with 47kΩ pull-up resistors to 3.3V
 *
 * Build: pio run -e v5-diag -t upload && pio device monitor -e v5-diag
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

// ── Hardware Constants (from data_structures.h) ─────────────────
#define I2C_SDA             8
#define I2C_SCL             9
#define I2C_FREQ            400000   // 400 kHz
#define BNO085_ADDR         0x4B
#define ADS1115_ADDR_1      0x48     // ADDR→GND
#define ADS1115_ADDR_2      0x49     // ADDR→VDD
#define NUM_FLEX_SENSORS    5
#define SINGLE_HAND_FEATURES 11

// ── ADS1115 Register Constants ──────────────────────────────────
#define ADS1115_REG_CONV    0x00
#define ADS1115_REG_CFG     0x01
#define ADS1115_OS_START    0x8000
#define ADS1115_CFG_BASE    0x0183   // PGA=±4.096V, single-shot, 128SPS

// ── Timing ──────────────────────────────────────────────────────
#define BNO085_BOOT_MS      2000
#define ADS1115_CONV_MS     12
#define STABILITY_SAMPLES   50
#define NOISE_SAMPLES       100
#define DATA_COLLECT_MS     2000

// ── Thresholds ──────────────────────────────────────────────────
#define QUAT_TOLERANCE      0.15f
#define GYRO_ACTIVE_MIN     0.05f    // rad/s — gyro is "alive" if |ω| > this
#define FLEX_NOISE_MAX      0.15f    // σ must be below this
#define FLEX_RESP_MIN       0.08f    // min range for "responsive" detection
#define I2C_SUCCESS_MIN     0.95f    // 95% must succeed

// ── Flex sensor names ───────────────────────────────────────────
static const char* FLEX_NAMES[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};

// ── Global state ────────────────────────────────────────────────
static Adafruit_BNO08x bno;
static bool bno_ok = false;
static int total_pass = 0;
static int total_fail = 0;

// ═══════════════════════════════════════════════════════════════════
// Output Helpers
// ═══════════════════════════════════════════════════════════════════

void printBanner(const char* title) {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.printf("║  %-54s ║\n", title);
    Serial.println("╚══════════════════════════════════════════════════════════╝");
}

void printPhase(const char* title) {
    Serial.println();
    Serial.println("┌──────────────────────────────────────────────────────────┐");
    Serial.printf("│  PHASE: %-48s │\n", title);
    Serial.println("└──────────────────────────────────────────────────────────┘");
}

void pass(const char* msg) {
    Serial.printf("  ✅ PASS: %s\n", msg);
    total_pass++;
}

void fail(const char* msg) {
    Serial.printf("  ❌ FAIL: %s\n", msg);
    total_fail++;
}

void info(const char* msg) {
    Serial.printf("  ℹ️  %s\n", msg);
}

void warn(const char* msg) {
    Serial.printf("  ⚠️  WARN: %s\n", msg);
}

// ═══════════════════════════════════════════════════════════════════
// Math Helpers
// ═══════════════════════════════════════════════════════════════════

float stddev(const float* data, int n) {
    if (n < 2) return 0.0f;
    float sum = 0, sum2 = 0;
    for (int i = 0; i < n; i++) { sum += data[i]; sum2 += data[i] * data[i]; }
    float mean = sum / n;
    float var = sum2 / n - mean * mean;
    return sqrtf(var > 0 ? var : 0);
}

void quatToEuler(const float q[4], float euler[3]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    // Roll (X-axis)
    float sinr = 2.0f * (w * x + y * z);
    float cosr = 1.0f - 2.0f * (x * x + y * y);
    euler[0] = atan2f(sinr, cosr) * 57.2958f;
    // Pitch (Y-axis)
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f)
        euler[1] = copysignf(90.0f, sinp);
    else
        euler[1] = asinf(sinp) * 57.2958f;
    // Yaw (Z-axis)
    float siny = 2.0f * (w * z + x * y);
    float cosy = 1.0f - 2.0f * (y * y + z * z);
    euler[2] = atan2f(siny, cosy) * 57.2958f;
}

// ═══════════════════════════════════════════════════════════════════
// I2C Helpers
// ═══════════════════════════════════════════════════════════════════

const char* deviceLabel(uint8_t addr) {
    switch (addr) {
        case ADS1115_ADDR_1: return "ADS1115 #1 (Flex 0-2)";
        case ADS1115_ADDR_2: return "ADS1115 #2 (Flex 3-4)";
        case BNO085_ADDR:    return "BNO085 IMU";
        case 0x4A:           return "BNO085 (alt addr)";
        default:             return nullptr;
    }
}

int scanI2CBus(uint8_t* found, int max_found) {
    int count = 0;
    Serial.println("  Scanning 0x03–0x77...");
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            const char* lbl = deviceLabel(addr);
            Serial.printf("    [0x%02X] FOUND — %s\n", addr, lbl ? lbl : "Unknown device");
            if (count < max_found) found[count++] = addr;
        }
        // Progress every 16 addresses
        if ((addr & 0x0F) == 0x0F) {
            Serial.printf("    ...scanned up to 0x%02X\n", addr);
        }
    }
    return count;
}

bool i2cPing(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// ═══════════════════════════════════════════════════════════════════
// ADS1115 Raw Read
// ═══════════════════════════════════════════════════════════════════

int16_t ads1115ReadRaw(uint8_t addr, int channel) {
    // Build config: OS=start, MUX=AINx-vs-GND
    // MUX bits [14:12]: 100=AIN0, 101=AIN1, 110=AIN2, 111=AIN3
    uint16_t config = ADS1115_OS_START | ADS1115_CFG_BASE | ((uint16_t)(4 + channel) << 12);

    // Write config register
    Wire.beginTransmission(addr);
    Wire.write(ADS1115_REG_CFG);
    Wire.write((uint8_t)(config >> 8));
    Wire.write((uint8_t)(config & 0xFF));
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("    ADS1115 CFG write err=%d at 0x%02X ch=%d\n", err, addr, channel);
        return -1;
    }

    // Wait for conversion (128 SPS ≈ 8ms, use 12ms)
    delay(ADS1115_CONV_MS);

    // Point to conversion register
    Wire.beginTransmission(addr);
    Wire.write(ADS1115_REG_CONV);
    err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("    ADS1115 CONV ptr err=%d at 0x%02X ch=%d\n", err, addr, channel);
        return -1;
    }

    // Read 2 bytes
    uint8_t n = Wire.requestFrom(addr, (uint8_t)2);
    if (n >= 2) {
        int16_t raw = (Wire.read() << 8) | Wire.read();
        return raw;
    }
    Serial.printf("    ADS1115 READ short: got %d bytes at 0x%02X ch=%d\n", n, addr, channel);
    return -1;
}

float ads1115ReadVoltage(uint8_t addr, int channel) {
    int16_t raw = ads1115ReadRaw(addr, channel);
    if (raw < 0) return -1.0f;
    // PGA ±4.096V → 0.125mV per count → V = raw * 0.000125
    return (float)raw * 0.000125f;
}

float ads1115ReadNormalized(uint8_t addr, int channel) {
    int16_t raw = ads1115ReadRaw(addr, channel);
    if (raw < 0) return -1.0f;
    // Normalize to [0, 1] using raw ADC range
    // Min: ~260 counts (0.1V, sensor straight, low resistance)
    // Max: ~25600 counts (3.2V, sensor bent, high resistance)
    static constexpr float RAW_MIN = 260.0f;
    static constexpr float RAW_MAX = 25600.0f;
    float n = ((float)raw - RAW_MIN) / (RAW_MAX - RAW_MIN);
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    return n;
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 0: System Info
// ═══════════════════════════════════════════════════════════════════

void phase0_systemInfo() {
    printPhase("0: System Info");

    Serial.printf("  Chip Model:    %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("  CPU Freq:      %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  Flash Size:    %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("  PSRAM Size:    %d MB\n", ESP.getPsramSize() / (1024 * 1024));
    Serial.printf("  Free Heap:     %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  SDK Version:   %s\n", ESP.getSdkVersion());

    // Verify chip is ESP32-S3
    if (strstr(ESP.getChipModel(), "ESP32-S3") != nullptr) {
        pass("Chip is ESP32-S3");
    } else {
        fail("NOT ESP32-S3! This firmware requires ESP32-S3.");
    }

    // Verify PSRAM
    if (ESP.getPsramSize() >= 8 * 1024 * 1024) {
        pass("PSRAM >= 8 MB (N16R8 confirmed)");
    } else if (ESP.getPsramSize() >= 2 * 1024 * 1024) {
        warn("PSRAM present but < 8 MB — may be N8R2 or N8R8");
    } else {
        fail("NO PSRAM detected! Check board_build.psram and memory_type.");
    }
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 1: I2C Bus Scan
// ═══════════════════════════════════════════════════════════════════

void phase1_i2cScan() {
    printPhase("1: I2C Bus Scan");

    // STEP A: Check bus idle levels BEFORE I2C init
    // Use digitalRead on GPIO pins directly (pins default to INPUT after reset)
    int sda_lvl = digitalRead(I2C_SDA);
    int scl_lvl = digitalRead(I2C_SCL);
    Serial.printf("  Bus idle state (pre-I2C): SDA=%d SCL=%d\n", sda_lvl, scl_lvl);
    if (sda_lvl == HIGH && scl_lvl == HIGH) {
        pass("I2C bus lines HIGH (idle OK)");
    } else if (sda_lvl == LOW || scl_lvl == LOW) {
        fail("I2C bus line(s) stuck LOW! Check pull-up resistors.");
        // Bus recovery attempt (bit-bang 9 clock pulses)
        info("Attempting bus recovery (9 clock pulses on GPIO9)...");
        pinMode(I2C_SCL, OUTPUT);
        for (int i = 0; i < 9; i++) {
            digitalWrite(I2C_SCL, HIGH);
            delayMicroseconds(5);
            digitalWrite(I2C_SCL, LOW);
            delayMicroseconds(5);
        }
        // Restore to INPUT for I2C init
        pinMode(I2C_SCL, INPUT);
        delay(5);
        sda_lvl = digitalRead(I2C_SDA);
        scl_lvl = digitalRead(I2C_SCL);
        Serial.printf("  After recovery: SDA=%d SCL=%d\n", sda_lvl, scl_lvl);
    }

    // STEP B: Init I2C — MUST be AFTER bus line check
    // pinMode() overrides Wire.begin() config, so we check bus first, then init I2C
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
    Wire.setTimeOut(50);
    info("I2C initialized: SDA=GPIO8, SCL=GPIO9, 400kHz");

    // Full scan
    uint8_t found[16];
    int n = scanI2CBus(found, 16);
    Serial.printf("  Total devices found: %d (expected: 3)\n", n);

    if (n >= 3) {
        pass("Device count >= 3");
    } else if (n > 0) {
        warn("Fewer than 3 devices detected");
    } else {
        fail("NO I2C devices found! Check wiring and power.");
    }

    // Verify specific devices
    bool has_0x48 = false, has_0x49 = false, has_0x4B = false;
    for (int i = 0; i < n; i++) {
        if (found[i] == ADS1115_ADDR_1) has_0x48 = true;
        if (found[i] == ADS1115_ADDR_2) has_0x49 = true;
        if (found[i] == BNO085_ADDR)    has_0x4B = true;
    }

    if (has_0x48) pass("ADS1115 #1 detected @ 0x48 (ADDR→GND)");
    else          fail("ADS1115 #1 NOT detected @ 0x48 — check ADDR→GND, power");

    if (has_0x49) pass("ADS1115 #2 detected @ 0x49 (ADDR→VDD)");
    else          fail("ADS1115 #2 NOT detected @ 0x49 — check ADDR→VDD, power");

    if (has_0x4B) pass("BNO085 detected @ 0x4B (ADO→3.3V)");
    else          fail("BNO085 NOT detected @ 0x4B — check PS0=GND, PS1=GND, ADO=3.3V");

    // Warn if unexpected devices found
    for (int i = 0; i < n; i++) {
        if (found[i] != ADS1115_ADDR_1 && found[i] != ADS1115_ADDR_2 && found[i] != BNO085_ADDR) {
            Serial.printf("  ⚠️  Unexpected device at 0x%02X — bus conflict?\n", found[i]);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 2: ADS1115 #1 (0x48) — Flex Sensors 0-2
// ═══════════════════════════════════════════════════════════════════

void phase2_ads1115_1() {
    printPhase("2: ADS1115 #1 (0x48, ADDR→GND) — Flex 0-2");

    if (!i2cPing(ADS1115_ADDR_1)) {
        fail("ADS1115 #1 (0x48) not responding — skipping");
        return;
    }
    pass("ADS1115 #1 I2C ACK OK");

    // Read each channel
    struct { int ch; const char* name; } channels[] = {
        {0, "Thumb"}, {1, "Index"}, {2, "Middle"}
    };

    int ok_count = 0;
    for (auto& c : channels) {
        int16_t raw = ads1115ReadRaw(ADS1115_ADDR_1, c.ch);
        float v = ads1115ReadVoltage(ADS1115_ADDR_1, c.ch);
        float n = ads1115ReadNormalized(ADS1115_ADDR_1, c.ch);

        if (raw >= 0) {
            Serial.printf("  AIN%d (%s): raw=%5d  V=%.3f  norm=%.3f\n",
                          c.ch, c.name, raw, v, n);
            // Sanity check: voltage should be 0.0–3.3V
            if (v >= 0.0f && v <= 3.3f) {
                ok_count++;
            } else {
                warn("Voltage out of expected range [0, 3.3V]");
            }
        } else {
            Serial.printf("  AIN%d (%s): READ FAILED (err=%d)\n", c.ch, c.name, raw);
        }
        delay(20);
    }

    if (ok_count == 3) pass("ADS1115 #1: all 3 channels readable");
    else if (ok_count > 0) warn("ADS1115 #1: some channels failed");
    else fail("ADS1115 #1: NO channels readable — check flex sensor wiring");
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 3: ADS1115 #2 (0x49) — Flex Sensors 3-4
// ═══════════════════════════════════════════════════════════════════

void phase3_ads1115_2() {
    printPhase("3: ADS1115 #2 (0x49, ADDR→VDD) — Flex 3-4");

    if (!i2cPing(ADS1115_ADDR_2)) {
        fail("ADS1115 #2 (0x49) not responding — skipping");
        return;
    }
    pass("ADS1115 #2 I2C ACK OK");

    struct { int ch; const char* name; } channels[] = {
        {0, "Ring"}, {1, "Pinky"}
    };

    int ok_count = 0;
    for (auto& c : channels) {
        int16_t raw = ads1115ReadRaw(ADS1115_ADDR_2, c.ch);
        float v = ads1115ReadVoltage(ADS1115_ADDR_2, c.ch);
        float n = ads1115ReadNormalized(ADS1115_ADDR_2, c.ch);

        if (raw >= 0) {
            Serial.printf("  AIN%d (%s): raw=%5d  V=%.3f  norm=%.3f\n",
                          c.ch, c.name, raw, v, n);
            if (v >= 0.0f && v <= 3.3f) {
                ok_count++;
            } else {
                warn("Voltage out of expected range [0, 3.3V]");
            }
        } else {
            Serial.printf("  AIN%d (%s): READ FAILED (err=%d)\n", c.ch, c.name, raw);
        }
        delay(20);
    }

    if (ok_count == 2) pass("ADS1115 #2: both channels readable");
    else if (ok_count > 0) warn("ADS1115 #2: some channels failed");
    else fail("ADS1115 #2: NO channels readable — check flex sensor wiring");
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 4: BNO085 Initialization
// ═══════════════════════════════════════════════════════════════════

void phase4_bno085_init() {
    printPhase("4: BNO085 Initialization (0x4B)");

    if (!i2cPing(BNO085_ADDR)) {
        fail("BNO085 (0x4B) not on I2C bus — cannot initialize");
        bno_ok = false;
        return;
    }
    pass("BNO085 I2C ACK OK");

    info("Calling bno.begin_I2C(0x4B, &Wire)...");
    uint32_t t0 = millis();
    bno_ok = bno.begin_I2C(BNO085_ADDR, &Wire);
    uint32_t dt = millis() - t0;

    if (bno_ok) {
        Serial.printf("  BNO085 init SUCCESS (%d ms)\n", dt);
        pass("BNO085 begin_I2C() succeeded");
    } else {
        Serial.printf("  BNO085 init FAILED (%d ms)\n", dt);
        fail("BNO085 begin_I2C() failed — try power cycling the module");
        Serial.println("  TROUBLESHOOTING:");
        Serial.println("    1. Disconnect BNO085 VCC for 10 seconds");
        Serial.println("    2. Reconnect VCC");
        Serial.println("    3. Verify PS0=GND, PS1=GND, ADO=3.3V, CS=3.3V");
        Serial.println("    4. Re-run this test");
        return;
    }

    // Enable sensor reports
    info("Enabling sensor reports...");

    // Game Rotation Vector (6-axis: accel + gyro, no mag) @ 200Hz
    if (bno.enableReport(SH2_GAME_ROTATION_VECTOR, 5000)) {
        pass("Game Rotation Vector enabled (200Hz)");
    } else {
        fail("Game Rotation Vector enable FAILED");
    }

    // Accelerometer @ 100Hz
    if (bno.enableReport(SH2_ACCELEROMETER, 10000)) {
        pass("Accelerometer enabled (100Hz)");
    } else {
        warn("Accelerometer enable FAILED (non-critical)");
    }

    // Gyroscope Calibrated @ 100Hz
    if (bno.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000)) {
        pass("Gyroscope (calibrated) enabled (100Hz)");
    } else {
        warn("Gyroscope enable FAILED (non-critical)");
    }

    info("Waiting 500ms for first sensor reports to arrive...");
    delay(500);
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 5: BNO085 Data Stream
// ═══════════════════════════════════════════════════════════════════

void phase5_bno085_data() {
    printPhase("5: BNO085 Data Stream");

    if (!bno_ok) {
        fail("BNO085 not initialized — skipping data tests");
        return;
    }

    float quat[4] = {1, 0, 0, 0};
    float euler[3] = {0, 0, 0};
    float gyro[3] = {0, 0, 0};
    float accel[3] = {0, 0, 0};
    bool got_grv = false, got_gyro = false, got_accel = false;
    int grv_count = 0, gyro_count = 0, accel_count = 0;

    uint32_t deadline = millis() + DATA_COLLECT_MS;
    while (millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            switch (val.sensorId) {
                case SH2_GAME_ROTATION_VECTOR:
                    quat[0] = val.un.gameRotationVector.real;
                    quat[1] = val.un.gameRotationVector.i;
                    quat[2] = val.un.gameRotationVector.j;
                    quat[3] = val.un.gameRotationVector.k;
                    got_grv = true;
                    grv_count++;
                    break;
                case SH2_GYROSCOPE_CALIBRATED:
                    gyro[0] = val.un.gyroscope.x;
                    gyro[1] = val.un.gyroscope.y;
                    gyro[2] = val.un.gyroscope.z;
                    got_gyro = true;
                    gyro_count++;
                    break;
                case SH2_ACCELEROMETER:
                    accel[0] = val.un.accelerometer.x;
                    accel[1] = val.un.accelerometer.y;
                    accel[2] = val.un.accelerometer.z;
                    got_accel = true;
                    accel_count++;
                    break;
            }
        }
        delay(1);
    }

    // ── Report counts ──
    Serial.printf("  GRV samples:   %d\n", grv_count);
    Serial.printf("  Gyro samples:  %d\n", gyro_count);
    Serial.printf("  Accel samples: %d\n", accel_count);

    if (grv_count > 10) pass("Game Rotation Vector streaming OK");
    else if (grv_count > 0) warn("GRV data rate low");
    else fail("NO Game Rotation Vector data! Sensor may need calibration");

    if (gyro_count > 10) pass("Gyroscope streaming OK");
    else if (gyro_count > 0) warn("Gyro data rate low");
    else warn("No gyro data (may need movement to trigger)");

    if (accel_count > 10) pass("Accelerometer streaming OK");
    else if (accel_count > 0) warn("Accel data rate low");
    else warn("No accel data");

    // ── Quaternion unit check ──
    if (got_grv) {
        float mag = quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3];
        float dev = fabsf(mag - 1.0f);
        Serial.printf("  Quaternion: [%.4f, %.4f, %.4f, %.4f]  |q|²=%.4f (dev=%.4f)\n",
                      quat[0], quat[1], quat[2], quat[3], mag, dev);
        if (dev < QUAT_TOLERANCE) {
            pass("Quaternion is unit (valid)");
        } else {
            warn("Quaternion not unit — sensor may need calibration time");
        }

        quatToEuler(quat, euler);
        Serial.printf("  Euler: Roll=%.1f°  Pitch=%.1f°  Yaw=%.1f°\n",
                      euler[0], euler[1], euler[2]);
        if (euler[0] >= -180 && euler[0] <= 180 &&
            euler[1] >= -90  && euler[1] <= 90  &&
            euler[2] >= -180 && euler[2] <= 180) {
            pass("Euler angles in valid range");
        } else {
            warn("Euler angles out of expected range");
        }
    }

    // ── Gyroscope check ──
    if (got_gyro) {
        float gyro_mag = sqrtf(gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]);
        Serial.printf("  Gyro: [%.4f, %.4f, %.4f] rad/s  |ω|=%.4f\n",
                      gyro[0], gyro[1], gyro[2], gyro_mag);
        if (gyro_mag > GYRO_ACTIVE_MIN) {
            pass("Gyro not stuck at zero (sensor alive)");
        } else {
            warn("Gyro near zero — move the glove to verify");
        }
    }

    // ── Accelerometer check ──
    if (got_accel) {
        float accel_mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
        Serial.printf("  Accel: [%.3f, %.3f, %.3f] m/s²  |a|=%.3f (expect ~9.8)\n",
                      accel[0], accel[1], accel[2], accel_mag);
        if (accel_mag > 5.0f && accel_mag < 15.0f) {
            pass("Accel magnitude ~1g (reasonable)");
        } else {
            warn("Accel magnitude unusual — check sensor orientation");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 6: Flex Sensor Responsiveness (Interactive)
// ═══════════════════════════════════════════════════════════════════

void phase6_flex_responsive() {
    printPhase("6: Flex Sensor Responsiveness");

    bool ads1_ok = i2cPing(ADS1115_ADDR_1);
    bool ads2_ok = i2cPing(ADS1115_ADDR_2);

    if (!ads1_ok && !ads2_ok) {
        fail("Neither ADS1115 detected — skipping flex tests");
        return;
    }

    // Collect 20 samples over 3 seconds
    Serial.println("  Collecting flex data over 3 seconds...");
    Serial.println("  *** BEND EACH FINGER during this time! ***");

    float min_v[NUM_FLEX_SENSORS], max_v[NUM_FLEX_SENSORS];
    float sum_v[NUM_FLEX_SENSORS] = {0};
    int sample_count = 0;
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        min_v[i] = 999.0f;
        max_v[i] = -999.0f;
    }

    uint32_t deadline = millis() + 3000;
    while (millis() < deadline) {
        // Read all flex sensors
        float vals[NUM_FLEX_SENSORS];
        // ADS1115 #1: ch 0,1,2 → flex[0,1,2]
        // ADS1115 #2: ch 0,1   → flex[3,4]
        vals[0] = ads1115ReadNormalized(ADS1115_ADDR_1, 0);
        vals[1] = ads1115ReadNormalized(ADS1115_ADDR_1, 1);
        vals[2] = ads1115ReadNormalized(ADS1115_ADDR_1, 2);
        vals[3] = ads1115ReadNormalized(ADS1115_ADDR_2, 0);
        vals[4] = ads1115ReadNormalized(ADS1115_ADDR_2, 1);

        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            if (vals[i] >= 0) {
                if (vals[i] < min_v[i]) min_v[i] = vals[i];
                if (vals[i] > max_v[i]) max_v[i] = vals[i];
                sum_v[i] += vals[i];
            }
        }
        sample_count++;
        delay(50);
    }

    Serial.printf("  Samples collected: %d\n", sample_count);

    // Report each flex sensor
    int responsive_count = 0;
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        float range = max_v[i] - min_v[i];
        float avg = sum_v[i] / sample_count;
        Serial.printf("  Flex[%d] %s: range=%.3f (%.3f–%.3f) avg=%.3f\n",
                      i, FLEX_NAMES[i], range, min_v[i], max_v[i], avg);
        if (range >= FLEX_RESP_MIN) {
            Serial.printf("    ✅ Responsive (range %.3f >= %.2f)\n", range, FLEX_RESP_MIN);
            responsive_count++;
        } else {
            Serial.printf("    ⚠️  Not responsive (range %.3f < %.2f) — bend this finger!\n",
                          range, FLEX_RESP_MIN);
        }
    }

    if (responsive_count >= 4) {
        pass("Flex sensors: most responsive");
    } else if (responsive_count >= 2) {
        warn("Some flex sensors not responsive — bend them during test");
    } else {
        warn("Few flex sensors responsive — check pull-up resistors and wiring");
    }
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 7: Full Feature Array
// ═══════════════════════════════════════════════════════════════════

void phase7_featureArray() {
    printPhase("7: Full Feature Array (11-dim)");

    // Read flex sensors
    float flex[NUM_FLEX_SENSORS];
    flex[0] = ads1115ReadNormalized(ADS1115_ADDR_1, 0);
    flex[1] = ads1115ReadNormalized(ADS1115_ADDR_1, 1);
    flex[2] = ads1115ReadNormalized(ADS1115_ADDR_1, 2);
    flex[3] = ads1115ReadNormalized(ADS1115_ADDR_2, 0);
    flex[4] = ads1115ReadNormalized(ADS1115_ADDR_2, 1);

    // Read BNO085
    float quat[4] = {1, 0, 0, 0};
    float gyro[3] = {0, 0, 0};
    bool got_grv = false, got_gyro = false;

    if (bno_ok) {
        uint32_t deadline = millis() + 1000;
        while ((!got_grv || !got_gyro) && millis() < deadline) {
            sh2_SensorValue_t val;
            if (bno.getSensorEvent(&val)) {
                if (val.sensorId == SH2_GAME_ROTATION_VECTOR) {
                    quat[0] = val.un.gameRotationVector.real;
                    quat[1] = val.un.gameRotationVector.i;
                    quat[2] = val.un.gameRotationVector.j;
                    quat[3] = val.un.gameRotationVector.k;
                    got_grv = true;
                }
                if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                    gyro[0] = val.un.gyroscope.x;
                    gyro[1] = val.un.gyroscope.y;
                    gyro[2] = val.un.gyroscope.z;
                    got_gyro = true;
                }
            }
            delay(1);
        }
    }

    float euler[3] = {0, 0, 0};
    if (got_grv) quatToEuler(quat, euler);

    // Build feature array
    float features[SINGLE_HAND_FEATURES];
    memcpy(features, flex, 5 * sizeof(float));
    memcpy(features + 5, euler, 3 * sizeof(float));
    memcpy(features + 8, gyro, 3 * sizeof(float));

    // Print the feature array
    Serial.println("  Feature vector (11-dim):");
    Serial.printf("    Flex:  [");
    for (int i = 0; i < 5; i++) Serial.printf("%.3f%s", features[i], i < 4 ? ", " : "");
    Serial.println("]");
    Serial.printf("    Euler: [%.1f, %.1f, %.1f]\n", features[5], features[6], features[7]);
    Serial.printf("    Gyro:  [%.4f, %.4f, %.4f]\n", features[8], features[9], features[10]);

    // Validate ranges
    bool flex_ok = true;
    for (int i = 0; i < 5; i++) {
        if (features[i] < -0.1f || features[i] > 1.1f) flex_ok = false;
    }
    bool euler_ok = (features[5] >= -180 && features[5] <= 180 &&
                     features[6] >= -90  && features[6] <= 90  &&
                     features[7] >= -180 && features[7] <= 180);

    if (flex_ok) pass("Flex values in [0, 1] range");
    else warn("Some flex values out of range");

    if (euler_ok) pass("Euler angles in valid range");
    else warn("Euler angles out of range");

    pass("11-dim feature array assembled successfully");
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 8: Stability & Noise
// ═══════════════════════════════════════════════════════════════════

void phase8_stability() {
    printPhase("8: Stability & Noise");

    // ── 8a: I2C stability ──
    Serial.println("  --- I2C Stability (50 pings to 0x48) ---");
    int i2c_ok = 0;
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        Wire.beginTransmission(ADS1115_ADDR_1);
        if (Wire.endTransmission() == 0) i2c_ok++;
        delay(20);
    }
    float i2c_rate = (float)i2c_ok / STABILITY_SAMPLES;
    Serial.printf("  I2C success: %d/%d (%.1f%%)\n", i2c_ok, STABILITY_SAMPLES, i2c_rate * 100);
    if (i2c_rate >= I2C_SUCCESS_MIN) {
        pass("I2C stability >= 95%");
    } else {
        fail("I2C stability < 95% — check pull-up resistors and wiring");
    }

    // ── 8b: Flex noise ──
    Serial.println("  --- Flex Sensor Noise (100 samples) ---");
    float flex_data[NUM_FLEX_SENSORS][NOISE_SAMPLES];
    for (int s = 0; s < NOISE_SAMPLES; s++) {
        flex_data[0][s] = ads1115ReadNormalized(ADS1115_ADDR_1, 0);
        flex_data[1][s] = ads1115ReadNormalized(ADS1115_ADDR_1, 1);
        flex_data[2][s] = ads1115ReadNormalized(ADS1115_ADDR_1, 2);
        flex_data[3][s] = ads1115ReadNormalized(ADS1115_ADDR_2, 0);
        flex_data[4][s] = ads1115ReadNormalized(ADS1115_ADDR_2, 1);
        delay(10);
    }
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        float sd = stddev(flex_data[i], NOISE_SAMPLES);
        Serial.printf("  Flex[%d] %s: σ=%.4f (max %.2f)\n", i, FLEX_NAMES[i], sd, FLEX_NOISE_MAX);
        if (sd <= FLEX_NOISE_MAX) {
            pass("Flex noise OK");
        } else {
            warn("Flex noise high — check sensor connections");
        }
    }

    // ── 8c: Gyro noise (when stationary) ──
    if (bno_ok) {
        Serial.println("  --- Gyro Noise (when stationary, 100 samples) ---");
        float gx[NOISE_SAMPLES], gy[NOISE_SAMPLES], gz[NOISE_SAMPLES];
        int gyro_n = 0;
        uint32_t deadline = millis() + 5000;
        while (gyro_n < NOISE_SAMPLES && millis() < deadline) {
            sh2_SensorValue_t val;
            if (bno.getSensorEvent(&val)) {
                if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                    gx[gyro_n] = val.un.gyroscope.x;
                    gy[gyro_n] = val.un.gyroscope.y;
                    gz[gyro_n] = val.un.gyroscope.z;
                    gyro_n++;
                }
            }
            delay(1);
        }
        if (gyro_n >= NOISE_SAMPLES / 2) {
            float sx = stddev(gx, gyro_n);
            float sy = stddev(gy, gyro_n);
            float sz = stddev(gz, gyro_n);
            Serial.printf("  Gyro σ: [%.4f, %.4f, %.4f] rad/s (n=%d)\n", sx, sy, sz, gyro_n);
            if (sx < 0.5f && sy < 0.5f && sz < 0.5f) {
                pass("Gyro noise within acceptable range");
            } else {
                warn("Gyro noise high — is the glove moving?");
            }
        } else {
            warn("Insufficient gyro samples for noise measurement");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Streaming Mode (POST-TEST)
// ═══════════════════════════════════════════════════════════════════

void streamSensorData() {
    printBanner("LIVE DATA STREAM (Ctrl+C to stop)");

    Serial.println("  Format: timestamp_ms, flex[0..4], euler[0..2], gyro[0..2]");
    Serial.println("  ─────────────────────────────────────────────────────");

    while (true) {
        uint32_t ts = millis();

        // Read flex
        float flex[5];
        flex[0] = ads1115ReadNormalized(ADS1115_ADDR_1, 0);
        flex[1] = ads1115ReadNormalized(ADS1115_ADDR_1, 1);
        flex[2] = ads1115ReadNormalized(ADS1115_ADDR_1, 2);
        flex[3] = ads1115ReadNormalized(ADS1115_ADDR_2, 0);
        flex[4] = ads1115ReadNormalized(ADS1115_ADDR_2, 1);

        // Read BNO085
        float euler[3] = {0, 0, 0};
        float gyro[3] = {0, 0, 0};
        if (bno_ok) {
            sh2_SensorValue_t val;
            while (bno.getSensorEvent(&val)) {
                if (val.sensorId == SH2_GAME_ROTATION_VECTOR) {
                    float q[4] = {
                        val.un.gameRotationVector.real,
                        val.un.gameRotationVector.i,
                        val.un.gameRotationVector.j,
                        val.un.gameRotationVector.k
                    };
                    quatToEuler(q, euler);
                }
                if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                    gyro[0] = val.un.gyroscope.x;
                    gyro[1] = val.un.gyroscope.y;
                    gyro[2] = val.un.gyroscope.z;
                }
            }
        }

        // Print CSV line
        Serial.printf("%d,", ts);
        for (int i = 0; i < 5; i++) Serial.printf("%.3f,", flex[i]);
        for (int i = 0; i < 3; i++) Serial.printf("%.1f,", euler[i]);
        for (int i = 0; i < 3; i++) Serial.printf("%.4f%s", gyro[i], i < 2 ? "," : "");
        Serial.println();

        delay(50);  // ~20Hz data rate
    }
}

// ═══════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════

void setup() {
    delay(BNO085_BOOT_MS);  // Let BNO085 boot

    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);

    printBanner("EchoGlove V5 — Comprehensive Hardware Diagnostic v3");
    Serial.println("  DualGloveFlex Branch | ESP32-S3-N16R8");
    Serial.println("  Wiring: SDA=GPIO8, SCL=GPIO9, 4.7kΩ pull-ups");
    Serial.println("  Expected: BNO085@0x4B + ADS1115@0x48 + ADS1115@0x49");
    Serial.println("==============================================================");

    // ── Run all phases ──
    phase0_systemInfo();
    phase1_i2cScan();
    phase2_ads1115_1();
    phase3_ads1115_2();
    phase4_bno085_init();
    phase5_bno085_data();
    phase6_flex_responsive();
    phase7_featureArray();
    phase8_stability();

    // ── Summary ──
    printBanner("DIAGNOSTIC SUMMARY");
    Serial.printf("  ✅ PASS: %d\n", total_pass);
    Serial.printf("  ❌ FAIL: %d\n", total_fail);
    Serial.printf("  Total:  %d checks\n", total_pass + total_fail);

    if (total_fail == 0) {
        Serial.println();
        Serial.println("  🎉 ALL CHECKS PASSED! Hardware is ready for data collection.");
    } else if (total_fail <= 3) {
        Serial.println();
        Serial.printf("  ⚠️  %d check(s) failed — review above for details.\n", total_fail);
    } else {
        Serial.println();
        Serial.printf("  🚨 %d checks failed — significant hardware issues detected.\n", total_fail);
        Serial.println("  Please check wiring and re-run diagnostic.");
    }

    // ── Stream data continuously ──
    Serial.println();
    Serial.println("══════════════════════════════════════════════════════════════");
    Serial.println("  Starting continuous data stream in 3 seconds...");
    Serial.println("  Press Ctrl+C in monitor to stop, or reset to re-run tests.");
    Serial.println("══════════════════════════════════════════════════════════════");
    delay(3000);

    streamSensorData();
}

void loop() {
    // Never reached — streamSensorData() runs forever
    delay(1000);
}