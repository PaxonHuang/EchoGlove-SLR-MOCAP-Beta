/* =============================================================================
 * EchoGlove V5.2 — BNO085 Diagnostic Firmware (isolated, no ADS1115)
 * =============================================================================
 * Hardware: ESP32-S3-DevKitC-1 N16R8 + GY-BNO085 (only)
 *   SDA  = GPIO8   (4.7 kΩ pull-up to 3V3)
 *   SCL  = GPIO9   (4.7 kΩ pull-up to 3V3)
 *   INT  = GPIO1   (BNO085 INT, falling edge)   — optional, can be -1
 *   RST  = 3V3     (hardwired; do NOT connect to any GPIO)
 *   ADO  = 3V3     (I²C address 0x4B)
 *   PS0  = 3V3     (protocol = I²C)
 *   PS1  = GND
 *   CS   = floating
 * =============================================================================
 * Build : pio run
 * Upload: pio run -t upload --upload-port COMx
 * Mon   : pio device monitor -b 115200
 * =============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

// ── SOP pin map ─────────────────────────────────────────────
static constexpr uint8_t  I2C_SDA         = 8;
static constexpr uint8_t  I2C_SCL         = 9;
static constexpr uint32_t I2C_FREQ_HZ     = 100000;   // 100 kHz
static constexpr int8_t   BNO085_INT_PIN  = 1;        // -1 if hardwired/floating
static constexpr int8_t   BNO085_RST_PIN  = -1;       // RST hardwired to 3V3
static constexpr uint8_t  BNO085_ADDR     = 0x4B;

// ── Driver (Adafruit_BNO08x ctor takes RST pin) ─────────────
Adafruit_BNO08x bno(BNO085_RST_PIN);

// ── helpers ─────────────────────────────────────────────────
static void banner(const char* s) {
    Serial.println();
    Serial.println("========================================");
    Serial.printf("  %s\n", s);
    Serial.println("========================================");
}

static void bus_recovery() {
    // 9 SCL clock pulses (BNO085 datasheet §1.4.4)
    pinMode(I2C_SCL, OUTPUT_OPEN_DRAIN);
    digitalWrite(I2C_SCL, HIGH);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL, LOW);  delayMicroseconds(5);
        digitalWrite(I2C_SCL, HIGH); delayMicroseconds(5);
    }
    pinMode(I2C_SCL, INPUT);
}

static void print_pin_map() {
    Serial.printf("SDA=GPIO%d  SCL=GPIO%d  INT=GPIO%d  RST=%s  ADDR=0x%02X  I2C=%lu Hz\n",
                  I2C_SDA, I2C_SCL, BNO085_INT_PIN,
                  (BNO085_RST_PIN < 0) ? "HARDWIRED" : String(BNO085_INT_PIN).c_str(),
                  BNO085_ADDR, (unsigned long)I2C_FREQ_HZ);
}

// ── PHASE 0: electrical sanity ─────────────────────────────
static void phase0_electrical() {
    banner("PHASE 0 - ELECTRICAL SANITY");
    print_pin_map();
    Serial.println("Manual checks before code test:");
    Serial.println("  [ ] BNO085 VCC between 3.20 and 3.35 V (loaded)");
    Serial.println("  [ ] SDA pull-up 4.7k to 3V3");
    Serial.println("  [ ] SCL pull-up 4.7k to 3V3");
    Serial.println("  [ ] PS0=3V3, PS1=GND, ADO=3V3, RST=3V3");
    Serial.println("  [ ] ESP32-S3 GND == BNO085 GND");
    delay(200);
}

// ── PHASE 1: multi-frequency I²C scan ──────────────────────
static void scan_at(uint32_t freq_hz) {
    char buf[48];
    snprintf(buf, sizeof(buf), "PHASE 1 - I2C SCAN @ %lu kHz", (unsigned long)(freq_hz/1000));
    banner(buf);

    Wire.begin(I2C_SDA, I2C_SCL, freq_hz);
    delay(50);
    bus_recovery();

    uint8_t found = 0;
    for (uint8_t a = 0x03; a < 0x78; a++) {
        Wire.beginTransmission(a);
        uint8_t e = Wire.endTransmission();
        if (e == 0) {
            Serial.printf("  [ACK ] 0x%02X\n", a);
            found++;
        } else if (e == 4) {
            Serial.printf("  [ERR ] 0x%02X  (other error)\n", a);
        }
    }
    Serial.printf("  -> %u device(s) found @ %lu kHz\n", found, (unsigned long)(freq_hz/1000));
    Wire.end();
    delay(100);
}

static void phase1_scan_all() {
    scan_at(100000);
    scan_at(50000);
    scan_at(10000);
}

// ── PHASE 1.5: raw register read ───────────────────────────
static void phase15_raw_read() {
    banner("PHASE 1.5 - RAW READ @ 0x4B");
    Wire.begin(I2C_SDA, I2C_SCL, 100000);
    delay(50);
    bus_recovery();

    // BNO085 advert reg 0x00
    Wire.beginTransmission(0x4B);
    Wire.write(0x00);
    uint8_t e = Wire.endTransmission(false);
    if (e != 0) { Serial.printf("  [TX] err=%u (no ACK)\n", e); return; }

    size_t n = Wire.requestFrom(0x4B, (uint8_t)6, (uint8_t)true);
    Serial.printf("  [RX] %u bytes: ", n);
    while (Wire.available()) Serial.printf("%02X ", Wire.read());
    Serial.println();
    Wire.end();
    delay(100);
}

// ── PHASE 2: library init ──────────────────────────────────
static bool phase2_init() {
    banner("PHASE 2 - begin_I2C()");
    Wire.begin(I2C_SDA, I2C_SCL, 100000);
    delay(50);
    bus_recovery();
    delay(200);   // BNO085 boot settling

    if (!bno.begin_I2C(BNO085_ADDR, &Wire, BNO085_INT_PIN)) {
        Serial.println("  [BNO085] begin_I2C FAILED");
        Serial.println("  -> check: address / pull-ups / RST timing / PS0-PS1 / cable");
        return false;
    }
    Serial.println("  [BNO085] begin_I2C OK");

    if (!bno.enableReport(SH2_ROTATION_VECTOR, 10000)) {   // 100 Hz
        Serial.println("  [BNO085] enableReport ROTATION_VECTOR FAILED");
    } else {
        Serial.println("  [BNO085] enabled ROTATION_VECTOR @ 100 Hz");
    }
    if (!bno.enableReport(SH2_ACCELEROMETER, 20000)) {    // 50 Hz
        Serial.println("  [BNO085] enableReport ACCELEROMETER FAILED");
    } else {
        Serial.println("  [BNO085] enabled ACCELEROMETER @ 50 Hz");
    }
    return true;
}

// ── PHASE 3: stream ─────────────────────────────────────────
static void phase3_stream(uint32_t duration_ms) {
    banner("PHASE 3 - STREAM (10 s)");
    uint32_t t0 = millis(), n = 0;
    while (millis() - t0 < duration_ms) {
        if (bno.wasReset()) Serial.println("  [BNO085] wasReset()=true WARNING");

        sh2_SensorValue_t ev;
        if (bno.getSensorEvent(&ev)) {
            n++;
            switch (ev.sensorId) {
                case SH2_ROTATION_VECTOR:
                    Serial.printf("  [ROT ] i=%ld j=%ld k=%ld r=%ld st=%u t=%u\n",
                                  (long)(ev.un.rotationVector.i*1000),
                                  (long)(ev.un.rotationVector.j*1000),
                                  (long)(ev.un.rotationVector.k*1000),
                                  (long)(ev.un.rotationVector.real*1000),
                                  ev.status, ev.timestamp);
                    break;
                case SH2_ACCELEROMETER:
                    Serial.printf("  [ACC ] x=%.2f y=%.2f z=%.2f st=%u t=%u\n",
                                  ev.un.accelerometer.x,
                                  ev.un.accelerometer.y,
                                  ev.un.accelerometer.z,
                                  ev.status, ev.timestamp);
                    break;
                default:
                    Serial.printf("  [id=%u] st=%u t=%u\n", ev.sensorId, ev.status, ev.timestamp);
                    break;
            }
        }
        delay(5);
    }
    Serial.printf("  -> %u events in %u ms (%.1f Hz)\n",
                  n, duration_ms, (n*1000.0f)/duration_ms);
}

// ── SETUP / LOOP ────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("########################################");
    Serial.println("# EchoGlove V5.2 - BNO085 Diagnostic    #");
    Serial.println("# ESP32-S3-N16R8 + GY-BNO085 (isolated) #");
    Serial.println("########################################");

    phase0_electrical();
    phase1_scan_all();
    phase15_raw_read();

    if (phase2_init()) phase3_stream(10000);

    banner("DIAGNOSTIC COMPLETE");
    Serial.println("Please copy ALL output above for analysis.");
    Serial.println("Looping forever - hit RESET to re-run.");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }