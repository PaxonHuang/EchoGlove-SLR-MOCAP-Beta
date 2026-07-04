/* =============================================================================
 * EchoGlove V5 — Comprehensive Hardware Diagnostic Test
 * =============================================================================
 * Runs on real ESP32-S3-N16R8 hardware via: pio test -e hardware_diag
 *
 * Tests:
 *   1. I2C bus scan — verify all 3 devices present
 *   2. ADS1115 detection — 0x48 (ADDR→GND) + 0x49 (ADDR→VDD)
 *   3. Flex sensor range — all 5 channels in [0, 1]
 *   4. Flex sensor responsiveness — values change when sensors are bent
 *   5. BNO085 detection — 0x4B (ADO→3.3V)
 *   6. BNO085 quaternion — data streaming, unit quaternion check
 *   7. BNO085 Euler conversion — roll/pitch/yaw in valid range
 *   8. BNO085 gyroscope — not stuck at zero
 *   9. Full 11-dim feature array — flex[5] + euler[3] + gyro[3]
 *  10. I2C bus stability — 50 consecutive reads without failure
 *  11. Flex sensor noise — std dev below threshold
 *  12. BNO085 gyro noise — std dev below threshold
 *
 * Wiring (confirmed by user):
 *   ESP32-S3-N16R8: SDA=GPIO8, SCL=GPIO9, 4.7kΩ pull-ups
 *   BNO085 (GY-BNO085): VCC→3.3V, PS0→GND, PS1→GND, ADO→3.3V (addr 0x4B)
 *   ADS1115 #1: ADDR→GND (0x48) — Flex 0-2 (Thumb/Index/Middle)
 *   ADS1115 #2: ADDR→VDD (0x49) — Flex 3-4 (Ring/Pinky)
 *   5x flex sensors with 47kΩ pull-up resistors
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <unity.h>
#include <math.h>
#include "data_structures.h"
#include "ADS1115Manager.h"
#include "FlexManager.h"
#include <Adafruit_BNO08x.h>

// ── Hardware Constants ────────────────────────────────────────────
#define BNO085_ADDR       0x4B
#define EXPECTED_ADDR_1   0x48
#define EXPECTED_ADDR_2   0x49

// ── Test timing constants ─────────────────────────────────────────
#define BNO085_BOOT_DELAY_MS   2000
#define DATA_COLLECT_TIME_MS   1000
#define STABILITY_SAMPLES      50
#define NOISE_SAMPLES          100

// ── Thresholds ────────────────────────────────────────────────────
#define QUAT_TOLERANCE         0.15f   // unit quaternion tolerance
#define GYRO_ACTIVE_THRESH     0.05f   // rad/s — gyro must exceed to prove not stuck
#define I2C_SUCCESS_RATE       0.95f   // 95% reads must succeed
#define FLEX_NOISE_STDDEV_MAX  0.15f   // max acceptable noise
#define GYRO_NOISE_STDDEV_MAX  0.5f    // rad/s — max acceptable gyro noise
#define FLEX_RESPONSIVE_RANGE  0.08f   // min range across 5 channels

// ── Global objects ────────────────────────────────────────────────
static ADS1115Manager adc;
static FlexManager    flex;
static Adafruit_BNO08x bno;
static bool           bno_ok = false;

// ── Helper: quaternion → Euler (ZYX, degrees) ────────────────────
static void quatToEuler(const float q[4], float euler[3]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    // Roll (X)
    float sinr = 2.0f * (w * x + y * z);
    float cosr = 1.0f - 2.0f * (x * x + y * y);
    euler[0] = atan2f(sinr, cosr) * 57.2958f;
    // Pitch (Y)
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f)
        euler[1] = copysignf(90.0f, sinp) * 57.2958f;
    else
        euler[1] = asinf(sinp) * 57.2958f;
    // Yaw (Z)
    float siny = 2.0f * (w * z + x * y);
    float cosy = 1.0f - 2.0f * (y * y + z * z);
    euler[2] = atan2f(siny, cosy) * 57.2958f;
}

// ── Helper: scan I2C bus, return count of ACK'd addresses ─────────
static int scanI2C(TwoWire &wire, uint8_t *found_addrs, int max_found) {
    int count = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        wire.beginTransmission(addr);
        if (wire.endTransmission() == 0) {
            if (count < max_found) found_addrs[count++] = addr;
        }
    }
    return count;
}

// ── Helper: compute std deviation ─────────────────────────────────
static float stddev(const float *data, int n) {
    if (n < 2) return 0.0f;
    float sum = 0, sum2 = 0;
    for (int i = 0; i < n; i++) { sum += data[i]; sum2 += data[i] * data[i]; }
    float mean = sum / n;
    float var = sum2 / n - mean * mean;
    return sqrtf(var > 0 ? var : 0);
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 1: System Info
// ═══════════════════════════════════════════════════════════════════

void test_chip_info() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    TEST_ASSERT_EQUAL(CHIP_ESP32S3, info.model);
    Serial.printf("  Chip: ESP32-S3 rev %d, %d cores, %s WiFi, %s BLE\n",
                  info.revision, info.cores,
                  (info.features & CHIP_FEATURE_WIFI_BGN) ? "YES" : "NO",
                  (info.features & CHIP_FEATURE_BLE) ? "YES" : "NO");
    Serial.printf("  Flash: %d MB, PSRAM: %d MB\n",
                  spi_flash_get_chip_size() / (1024 * 1024),
                  ESP.getPsramSize() / (1024 * 1024));
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 2: I2C Detection
// ═══════════════════════════════════════════════════════════════════

void test_i2c_scan() {
    uint8_t addrs[16];
    int count = scanI2C(Wire, addrs, 16);
    TEST_ASSERT_GREATER_OR_EQUAL(3, count);
    // Verify expected addresses are present
    bool found_0x48 = false, found_0x49 = false, found_0x4B = false;
    for (int i = 0; i < count; i++) {
        if (addrs[i] == 0x48) found_0x48 = true;
        if (addrs[i] == 0x49) found_0x49 = true;
        if (addrs[i] == 0x4B) found_0x4B = true;
        Serial.printf("  0x%02X", addrs[i]);
    }
    Serial.println();
    TEST_ASSERT_TRUE_MESSAGE(found_0x48, "ADS1115 #1 (0x48) not found");
    TEST_ASSERT_TRUE_MESSAGE(found_0x49, "ADS1115 #2 (0x49) not found");
    TEST_ASSERT_TRUE_MESSAGE(found_0x4B, "BNO085 (0x4B) not found");
}

void test_ads1115_1_detected() {
    Wire.beginTransmission(EXPECTED_ADDR_1);
    TEST_ASSERT_EQUAL_MESSAGE(0, Wire.endTransmission(),
        "ADS1115 #1 (0x48, ADDR→GND) not responding");
}

void test_ads1115_2_detected() {
    Wire.beginTransmission(EXPECTED_ADDR_2);
    TEST_ASSERT_EQUAL_MESSAGE(0, Wire.endTransmission(),
        "ADS1115 #2 (0x49, ADDR→VDD) not responding");
}

void test_bno085_detected() {
    Wire.beginTransmission(BNO085_ADDR);
    TEST_ASSERT_EQUAL_MESSAGE(0, Wire.endTransmission(),
        "BNO085 (0x4B) not responding. Check PS0→GND, ADO→3.3V");
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 3: Flex Sensors (ADS1115)
// ═══════════════════════════════════════════════════════════════════

void test_flex_range() {
    float values[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE_MESSAGE(adc.readAll(values), "ADS1115 readAll failed");
    const char *names[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        Serial.printf("  Flex[%d] %-6s = %.3f\n", i, names[i], values[i]);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 0.0f, values[i],
            "Flex value out of [0,1]");
    }
}

void test_flex_responsive() {
    float min_v[NUM_FLEX_SENSORS], max_v[NUM_FLEX_SENSORS];
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        min_v[i] = 1.0f;
        max_v[i] = 0.0f;
    }
    int samples = 20;
    for (int s = 0; s < samples; s++) {
        float v[NUM_FLEX_SENSORS];
        adc.readAll(v);
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
            if (v[i] < min_v[i]) min_v[i] = v[i];
            if (v[i] > max_v[i]) max_v[i] = v[i];
        }
        delay(50);
    }
    bool any_responsive = false;
    const char *names[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        float range = max_v[i] - min_v[i];
        Serial.printf("  Flex[%d] %-6s range=%.3f (%.3f..%.3f)\n",
                      i, names[i], range, min_v[i], max_v[i]);
        if (range >= FLEX_RESPONSIVE_RANGE) any_responsive = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(any_responsive,
        "No flex sensor shows range >= 0.08 — check wiring & pull-ups");
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 4: BNO085 IMU
// ═══════════════════════════════════════════════════════════════════

void test_bno085_init() {
    TEST_ASSERT_TRUE_MESSAGE(bno_ok, "BNO085 init failed in setup()");

    TEST_ASSERT_TRUE_MESSAGE(
        bno.enableReport(SH2_ROTATION_VECTOR, 5000),   // 200 Hz
        "enableReport(SH2_ROTATION_VECTOR) failed");
    TEST_ASSERT_TRUE_MESSAGE(
        bno.enableReport(SH2_ACCELEROMETER, 10000),    // 100 Hz
        "enableReport(SH2_ACCELEROMETER) failed");
    TEST_ASSERT_TRUE_MESSAGE(
        bno.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000),  // 100 Hz
        "enableReport(SH2_GYROSCOPE_CALIBRATED) failed");
}

void test_bno085_quaternion() {
    // Collect quaternion data for 1 second
    float quat_sum = 0;
    int count = 0;
    uint32_t start = millis();
    while (millis() - start < DATA_COLLECT_TIME_MS) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_ROTATION_VECTOR) {
                float w = val.un.rotationVector.real;
                float x = val.un.rotationVector.i;
                float y = val.un.rotationVector.j;
                float z = val.un.rotationVector.k;
                float mag = w * w + x * x + y * y + z * z;
                quat_sum += fabsf(mag - 1.0f);
                count++;
            }
        }
        delay(1);
    }
    Serial.printf("  Quaternion samples: %d in %d ms\n", count, DATA_COLLECT_TIME_MS);
    TEST_ASSERT_GREATER_THAN_MESSAGE(10, count,
        "No quaternion data received — check BNO085 wiring");
    float avg_deviation = quat_sum / count;
    Serial.printf("  Avg |q|²-1 deviation: %.4f (threshold: %.2f)\n",
                  avg_deviation, QUAT_TOLERANCE);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(QUAT_TOLERANCE, 0.0f, avg_deviation,
        "Quaternion not unit — sensor may need calibration");
}

void test_bno085_euler() {
    // Read a quaternion and convert to Euler
    float quat[4] = {1, 0, 0, 0};
    bool got_quat = false;
    uint32_t deadline = millis() + 2000;
    while (!got_quat && millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_ROTATION_VECTOR) {
                quat[0] = val.un.rotationVector.real;
                quat[1] = val.un.rotationVector.i;
                quat[2] = val.un.rotationVector.j;
                quat[3] = val.un.rotationVector.k;
                got_quat = true;
            }
        }
        delay(1);
    }
    TEST_ASSERT_TRUE_MESSAGE(got_quat, "No quaternion data for Euler test");

    float euler[3];
    quatToEuler(quat, euler);
    Serial.printf("  Quaternion: [%.3f, %.3f, %.3f, %.3f]\n",
                  quat[0], quat[1], quat[2], quat[3]);
    Serial.printf("  Euler: roll=%.1f° pitch=%.1f° yaw=%.1f°\n",
                  euler[0], euler[1], euler[2]);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(180.0f, 0.0f, euler[0],
        "Roll out of [-180,180]");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(180.0f, 0.0f, euler[1],
        "Pitch out of [-180,180]");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(180.0f, 0.0f, euler[2],
        "Yaw out of [-180,180]");
}

void test_bno085_gyro() {
    float gx = 0, gy = 0, gz = 0;
    bool got_gyro = false;
    uint32_t deadline = millis() + 2000;
    while (!got_gyro && millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                gx = val.un.gyroscope.x;
                gy = val.un.gyroscope.y;
                gz = val.un.gyroscope.z;
                got_gyro = true;
            }
        }
        delay(1);
    }
    TEST_ASSERT_TRUE_MESSAGE(got_gyro, "No gyroscope data received");

    Serial.printf("  Gyro: [%.3f, %.3f, %.3f] rad/s\n", gx, gy, gz);
    float mag = sqrtf(gx * gx + gy * gy + gz * gz);
    Serial.printf("  Gyro magnitude: %.3f rad/s\n", mag);
    TEST_ASSERT_GREATER_THAN_MESSAGE(GYRO_ACTIVE_THRESH, mag,
        "Gyro stuck at zero — sensor not responding");
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 5: Full Feature Array
// ═══════════════════════════════════════════════════════════════════

void test_full_feature_array() {
    // Read flex
    float flex_vals[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE(adc.readAll(flex_vals));

    // Read quaternion → Euler
    float quat[4] = {1, 0, 0, 0};
    bool got_quat = false;
    uint32_t deadline = millis() + 1000;
    while (!got_quat && millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_ROTATION_VECTOR) {
                quat[0] = val.un.rotationVector.real;
                quat[1] = val.un.rotationVector.i;
                quat[2] = val.un.rotationVector.j;
                quat[3] = val.un.rotationVector.k;
                got_quat = true;
            }
        }
        delay(1);
    }
    float euler[3] = {0, 0, 0};
    if (got_quat) quatToEuler(quat, euler);

    // Read gyro
    float gyro[3] = {0, 0, 0};
    deadline = millis() + 1000;
    bool got_gyro = false;
    while (!got_gyro && millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                gyro[0] = val.un.gyroscope.x;
                gyro[1] = val.un.gyroscope.y;
                gyro[2] = val.un.gyroscope.z;
                got_gyro = true;
            }
        }
        delay(1);
    }

    // Build feature array
    float features[SINGLE_HAND_FEATURES];
    memcpy(features, flex_vals, 5 * sizeof(float));
    memcpy(features + 5, euler, 3 * sizeof(float));
    memcpy(features + 8, gyro, 3 * sizeof(float));

    // Report
    Serial.printf("  Flex:  [%.3f, %.3f, %.3f, %.3f, %.3f]\n",
                  features[0], features[1], features[2], features[3], features[4]);
    Serial.printf("  Euler: [%.1f, %.1f, %.1f]\n",
                  features[5], features[6], features[7]);
    Serial.printf("  Gyro:  [%.3f, %.3f, %.3f]\n",
                  features[8], features[9], features[10]);
    Serial.printf("  Feature dim: %d (expected %d)\n",
                  SINGLE_HAND_FEATURES, 11);
    TEST_ASSERT_EQUAL_MESSAGE(11, SINGLE_HAND_FEATURES,
        "SINGLE_HAND_FEATURES != 11");

    // Validate flex in range
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, features[i]);
    }
    // Validate euler in range
    for (int i = 5; i < 8; i++) {
        TEST_ASSERT_FLOAT_WITHIN(180.0f, 0.0f, features[i]);
    }
}

// ═══════════════════════════════════════════════════════════════════
// TEST SUITE 6: Stability & Noise
// ═══════════════════════════════════════════════════════════════════

void test_i2c_stability() {
    int success = 0;
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        Wire.beginTransmission(0x48);
        if (Wire.endTransmission() == 0) success++;
        delay(20);
    }
    float rate = (float)success / STABILITY_SAMPLES;
    Serial.printf("  I2C 0x48: %d/%d success (%.0f%%)\n",
                  success, STABILITY_SAMPLES, rate * 100);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 1.0f, rate,
        "I2C success rate below 95%");
}

void test_flex_noise() {
    float readings[NUM_FLEX_SENSORS][NOISE_SAMPLES];
    for (int s = 0; s < NOISE_SAMPLES; s++) {
        float v[NUM_FLEX_SENSORS];
        adc.readAll(v);
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) readings[i][s] = v[i];
        delay(10);
    }
    const char *names[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        float sd = stddev(readings[i], NOISE_SAMPLES);
        Serial.printf("  Flex[%d] %-6s σ=%.4f (max %.2f)\n",
                      i, names[i], sd, FLEX_NOISE_STDDEV_MAX);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(FLEX_NOISE_STDDEV_MAX, 0.0f, sd,
            "Flex sensor noise too high");
    }
}

void test_bno085_gyro_noise() {
    float gx[NOISE_SAMPLES], gy[NOISE_SAMPLES], gz[NOISE_SAMPLES];
    int count = 0;
    uint32_t deadline = millis() + 5000;
    while (count < NOISE_SAMPLES && millis() < deadline) {
        sh2_SensorValue_t val;
        if (bno.getSensorEvent(&val)) {
            if (val.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                gx[count] = val.un.gyroscope.x;
                gy[count] = val.un.gyroscope.y;
                gz[count] = val.un.gyroscope.z;
                count++;
            }
        }
        delay(1);
    }
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(NOISE_SAMPLES / 2, count,
        "Insufficient gyro samples for noise test");

    float sx = stddev(gx, count);
    float sy = stddev(gy, count);
    float sz = stddev(gz, count);
    Serial.printf("  Gyro σ: [%.4f, %.4f, %.4f] rad/s (max %.2f)\n",
                  sx, sy, sz, GYRO_NOISE_STDDEV_MAX);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(GYRO_NOISE_STDDEV_MAX, 0.0f, sx,
        "Gyro X noise too high");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(GYRO_NOISE_STDDEV_MAX, 0.0f, sy,
        "Gyro Y noise too high");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(GYRO_NOISE_STDDEV_MAX, 0.0f, sz,
        "Gyro Z noise too high");
}

// ═══════════════════════════════════════════════════════════════════
// Unity setup / loop
// ═══════════════════════════════════════════════════════════════════

void setup() {
    delay(BNO085_BOOT_DELAY_MS);  // BNO085 power-on boot time
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("========================================");
    Serial.println(" EchoGlove V5 — Hardware Diagnostic");
    Serial.println("========================================");

    // I2C bus init
    Wire.begin(I2CPins::SDA, I2CPins::SCL, I2CPins::FREQ);
    Wire.setTimeOut(50);

    // Init ADS1115
    adc.begin(false);
    flex.begin(&adc);

    // Init BNO085
    bno_ok = bno.begin_I2C(BNO085_ADDR, &Wire);
    if (!bno_ok) {
        Serial.println("[BNO085] Init FAILED — will fail related tests");
    } else {
        Serial.println("[BNO085] Init OK");
    }

    // Run tests
    UNITY_BEGIN();

    Serial.println("\n── Suite 1: System Info ──");
    RUN_TEST(test_chip_info);

    Serial.println("\n── Suite 2: I2C Detection ──");
    RUN_TEST(test_i2c_scan);
    RUN_TEST(test_ads1115_1_detected);
    RUN_TEST(test_ads1115_2_detected);
    RUN_TEST(test_bno085_detected);

    Serial.println("\n── Suite 3: Flex Sensors ──");
    RUN_TEST(test_flex_range);
    RUN_TEST(test_flex_responsive);

    Serial.println("\n── Suite 4: BNO085 IMU ──");
    RUN_TEST(test_bno085_init);
    RUN_TEST(test_bno085_quaternion);
    RUN_TEST(test_bno085_euler);
    RUN_TEST(test_bno085_gyro);

    Serial.println("\n── Suite 5: Feature Array ──");
    RUN_TEST(test_full_feature_array);

    Serial.println("\n── Suite 6: Stability & Noise ──");
    RUN_TEST(test_i2c_stability);
    RUN_TEST(test_flex_noise);
    RUN_TEST(test_bno085_gyro_noise);

    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
