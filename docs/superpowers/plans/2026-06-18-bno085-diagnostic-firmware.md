# BNO085 Diagnostic Firmware & PlatformIO Board Fix — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore BNO085 I²C communication on ESP32-S3-N16R8 by fixing two SOP/board configuration bugs and deploying a comprehensive diagnostic firmware that systematically isolates hardware vs software faults.

**Architecture:** Two-config-fix (platformio.ini + main.cpp) + five-phase diagnostic sketch (electrical → multi-frequency scan → raw read → library init → 10s stream). All changes per project SOPs (`docs/HARDWARE_ASSEMBLY_GUIDE.md`, `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B_CN.md`) and BNO080 datasheet v1.3.

**Tech Stack:** PlatformIO (espressif32@^6.5.0), Arduino framework, Adafruit BNO08x v1.2.3, ESP32-S3-DevKitC-1-N16R8, GY-BNO085.

---

## Background — Two Confirmed Bugs

| # | Bug | Evidence | Impact |
|---|-----|----------|--------|
| 1 | `platformio.ini` configured as N8 (no PSRAM, 8MB flash) but env is named `-n16r8` and hardware is N16R8 (Octal PSRAM, 16MB flash) | `board_build.psram = disable`, `default_8MB.csv`, `qio_qspi` with `-DBOARD_HAS_PSRAM` | PSRAM not initialized → silent heap/buffer failures → possible Wire corruption |
| 2 | `main.cpp` uses GPIO11/12 for SDA/SCL (deviates from SOP GPIO8/9) and GPIO9 for INT (SOP GPIO1) | Comments in source: "ROUND 5 finding... GPIO1/GPIO2 known-good", "ROUND 10 trying GPIO11/12" | Wires to wrong pins; no physical contact with BNO085 → 0 devices found |

**SOP-correct pins** (per `docs/HARDWARE_ASSEMBLY_GUIDE.md` §4):
- I2C_SDA = 8, I2C_SCL = 9
- BNO085_INT_PIN = 1 (optional)
- BNO085_RST_PIN = -1 (RST hardwired to 3.3V)
- BNO085_ADDR = 0x4B

**SOP-correct board** (per espressif32 docs for N16R8):
- `board_build.partitions = default_16MB.csv`
- `board_build.psram = enable`
- `board_build.flash_mode = opi`
- `board_build.arduino.memory_type = qio_opi`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `glove_firmware/platformio.ini` | Modify | Correct N16R8 board config |
| `glove_firmware/src/main.cpp` | Overwrite | SOP-aligned 5-phase diagnostic |
| `PROGRESS.md` | Append | Document bugs found + fix history |
| `docs/HARDWARE_ASSEMBLY_GUIDE.md` | Read-only verify | Confirm SOP consistency |
| `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B_CN.md` | Read-only verify | Confirm SOP consistency |
| `docs/superpowers/plans/2026-06-18-bno085-diagnostic-firmware.md` | Create (this file) | Plan |

---

## Task 1: Fix `platformio.ini` for N16R8

**Files:**
- Modify: `glove_firmware/platformio.ini` (lines 14-19)

- [ ] **Step 1.1: Update board config block**

Replace lines 14-19:

```ini
; ---- Board-specific: ESP32-S3-DevKitC-1-N16R8 (16 MB Octal flash, 8 MB Octal PSRAM) ----
board_build.partitions = default_16MB.csv
board_build.psram      = enable
board_build.f_flash    = 80000000L
board_build.flash_mode = opi
board_build.arduino.memory_type = qio_opi
```

- [ ] **Step 1.2: Verify `-DBOARD_HAS_PSRAM` line**

Confirm line 23 `-DBOARD_HAS_PSRAM` is present (already there, now consistent with `psram = enable`).

- [ ] **Step 1.3: Commit**

```bash
git add glove_firmware/platformio.ini
git commit -m "fix(glove): correct N16R8 board config (Octal PSRAM, 16MB flash)"
```

---

## Task 2: Replace `main.cpp` with SOP-compliant diagnostic firmware

**Files:**
- Overwrite: `glove_firmware/src/main.cpp`

- [ ] **Step 2.1: Back up existing main.cpp**

```bash
cp glove_firmware/src/main.cpp glove_firmware/src/main.cpp.diag_v1
```

- [ ] **Step 2.2: Overwrite with SOP-compliant diagnostic**

Full contents (replace entire file):

```cpp
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
#include <Adafruit_BNO08x_SH2.h>

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
```

- [ ] **Step 2.3: Commit**

```bash
git add glove_firmware/src/main.cpp
git commit -m "fix(glove): SOP-compliant diagnostic firmware (GPIO8/9, 5-phase)"
```

---

## Task 3: Verify Build

- [ ] **Step 3.1: Run pio run**

```bash
cd glove_firmware && pio run
```

Expected: build succeeds (exit 0). If fails, read error output and adjust.

- [ ] **Step 3.2: Verify output**

Look for: `RAM:   [=         ]   X.X%` and `Flash: [=         ]  XX.X%`. No "Error:" lines.

---

## Task 4: Update PROGRESS.md

**Files:**
- Modify: `PROGRESS.md` (append section)

- [ ] **Step 4.1: Append diagnostic session section**

Append at end of `PROGRESS.md`:

```markdown

## 2026-06-18 — BNO085 Diagnostic Session (N16R8 + GY-BNO085)

### Bugs Found & Fixed

**Bug 1: `platformio.ini` wrong board config (CRITICAL)**
- Symptom: env named `-n16r8` but configured like N8 (no PSRAM, 8MB flash, qio)
- File: `glove_firmware/platformio.ini` lines 14-19
- Fix: `default_16MB.csv` + `psram=enable` + `flash_mode=opi` + `qio_opi`
- Impact: PSRAM was not initialized; heap/buffer failures could corrupt Wire state

**Bug 2: `main.cpp` pin violation vs SOP**
- Symptom: SDA=11, SCL=12, INT=9, RST=10 (all wrong)
- SOP requirement: SDA=8, SCL=9, INT=1, RST=-1 (hardwired 3V3)
- File: `glove_firmware/src/main.cpp`
- Fix: replaced with SOP-compliant diagnostic firmware

### Diagnostic Firmware Deployed
- 5 phases: electrical → multi-freq scan (100/50/10 kHz) → raw read → begin_I2C → 10s stream
- Located at `glove_firmware/src/main.cpp`
- Expected: Phase 1 detects 0x4B at all freqs; Phase 2 begin_I2C OK; Phase 3 ~30-100 events/10s

### Next Steps
1. Flash new firmware: `cd glove_firmware && pio run -t upload --upload-port COMx`
2. Monitor: `pio device monitor -b 115200`
3. Copy full output back to Claude for Phase-by-Phase analysis
4. If Phase 1 empty: re-verify wiring (4.7kΩ pull-ups, ADO=3V3, PS0=3V3, PS1=GND)
5. If Phase 2 fails: 5s power cycle then retry (BNO085 boot settling)
```

- [ ] **Step 4.2: Commit**

```bash
git add PROGRESS.md
git commit -m "docs(progress): log BNO085 diagnostic session + bugs found"
```

---

## Task 5: SOP Doc Review (Read-Only Verification)

- [ ] **Step 5.1: Verify `docs/HARDWARE_ASSEMBLY_GUIDE.md` says GPIO8/9 + 4.7kΩ**

Already confirmed in pre-plan read. No change needed.

- [ ] **Step 5.2: Verify `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B_CN.md` matches**

Already confirmed. No change needed.

- [ ] **Step 5.3: Decision — if any SOP doc contradicts fix**

STOP and surface the contradiction to user before committing.

---

## Self-Review

- Spec coverage: ✅ all 4 user asks (pin alignment, platformio.ini, main.cpp, PROGRESS.md) have tasks
- Placeholders: ✅ no "TBD" or "implement later"
- Type consistency: ✅ `bno.begin_I2C(addr, &Wire, int_pin)` matches Adafruit_BNO08x v1.2.3 API
- Commit cadence: ✅ 3 separate commits per logical unit

---

## Execution Handoff

Per user instruction "请直接开始执行" — executing in this session via `superpowers:executing-plans` skill. No further approval requested.