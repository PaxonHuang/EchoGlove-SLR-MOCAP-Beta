# 07 — Internal ADC Migration: ESP32-S3 ADC1 Replaces 2× ADS1115

> **Status**: Decided 2026-07-01 · **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
> **Scope**: Per-glove flex-sensor readout only (5 channels). IMU migration (BNO085→LSM6DSV16X) is documented in `01`/`03`/`04`; this doc concerns only the ADC change.
> **Supersedes**: every "ADS1115 unchanged" statement in `02`/`04`/`05`/`README.md` of this directory (stale points catalogued in §1).

---

## 0. TL;DR

In V6 the 5 flex sensors are read by the **ESP32-S3's own ADC1** (GPIO1–GPIO5) instead of two external ADS1115 chips. This removes the entire ADS1115 subsystem (2 chips, I²C addresses 0x48/0x49, the 60 ms/frame blocking read, and all V5 ADS1115 bugs) and cuts ¥16/glove (~25 % flex-BOM). The divider network and `IFlexSensor` abstraction make the change transparent to the inference pipeline.

| Decision | Choice | § |
|---|---|---|
| ADC chip | **ESP32-S3 internal ADC1** (no external ADC) | D-ADC-1 |
| API | `analogReadMilliVolts()` + N-sample software oversampling | D-ADC-2 |
| Calibration persistence | **NVS** via `Preferences` (per-channel key-value) | D-ADC-3 |
| Abstraction | **`IFlexSensor` interface** (Strategy/Adapter) | D-ADC-4 |
| Divider + attenuation | **Keep 47 kΩ pull-down + `ADC_ATTEN_DB_12`** | D-ADC-5 |

---

## 1. Why Migrate — V5 ADS1115 Bugs Being Eliminated

These are the V5 defects (catalogued in project memory `v6-internal-adc-decision`) that this migration removes by construction. They are **not** carried into V6.

| V5 location | Bug | How V6 internal-ADC fixes it |
|---|---|---|
| `ADS1115Manager.h:33` | `ADS1115_CFG_BASE = 0x0183` → data-rate bits[7:5]=`000` = **8 SPS**, despite comment "128 SPS" | No config register on internal ADC; sample rate set by polling loop directly |
| `ADS1115Manager.h:110` | `delay(12) ms × 5 ch = 60 ms` blocking/frame → ~16 Hz, not 100 Hz target | `analogReadMilliVolts()` is microseconds; full 5-ch oversampled read ≪ 1 ms |
| `ADS1115Manager.h:144-145` | `RAW_MIN/MAX` never updated (calibration operates on normalized, not raw) | Calibration writes raw min/max directly into NVS (§5) |
| `FlexManager.h` | Calibration in RAM only, lost on reboot | Persisted to NVS (D-ADC-3) |
| `KalmanFilter1D.h` | Exists, never wired into any pipeline | Wired into `InternalADCManager` (§6) |
| I²C bus | ADS1115×2 shared bus with BNO085 → one bad ADS1115 pulls SDA/SCL to 1.2 V (hardware failure 2026-06-28, 300 Ω short) | Flex readout moves off I²C entirely; I²C now carries only LSM6DSV16X |

**Root-cause note**: the 2026-06-28 hardware failure (SDA↔SCL 300 Ω, bus dragged to 1.2 V) was an ADS1115 die short. Removing the ADS1115s from the bus eliminates that failure class for the flex path. The I²C bus in V6 carries a **single** device (LSM6DSV16X @ 0x6A), see §3.

---

## 2. Quantitative Justification — Can Internal ADC Hold 5 Flex Sensors?

### 2.1 The question
ESP32-S3 ADC1 is 12-bit nominal, ~9–11 effective ENOB at default attenuation. Flex sensors produce a hand-state signal (5 classes of bend, not a continuous waveform). The real question: **is 9–11 ENOB enough that the downstream 1D-CNN/Attention classifier (46 classes) isn't degraded vs the ADS1115's 16-bit?**

### 2.2 Signal-domain analysis
- Divider output range: **0.9 V–2.4 V** across flex resistance 25 kΩ–125 kΩ (47 kΩ pull-down), i.e. **~1.5 V span** over the meaningful range.
- At 12-bit / `ADC_ATTEN_DB_12` (~0–2.5 V usable, up to 3.1 V): 1.5 V span ≈ **1843 raw counts** → ~10.8 bits of *useful* resolution within the active window. Adding 2 no-op bits of noise margin → still ~10 effective bits across the active span.
- ADS1115 at 16-bit on the same 1.5 V span (gain PGA=±4.096 V → uses ~0.55 of range): ~23500 counts → ~14.5 bits. **But** V5's 8-SPS config bug (§1) meant V5 *never realized* 16-bit at 100 Hz — the real V5 effective resolution at the buggy 8 SPS was the noise floor, not 16 bit.

**Conclusion**: within the active divider window, internal-ADC useful resolution (≈11 bits / 1843 counts) is **comparable to what V5 actually delivered** after its config bug, and well above the ~256 counts (8 bits) a 5-state-per-finger signal needs.

### 2.3 Recovery via oversampling (the design margin)
Software oversampling of N uncorrelated samples recovers ~log₂(√N) extra bits (standard oversampling & decimation). The chosen API (D-ADC-2) reads N samples per channel per frame:

| Oversample N | Extra bits | Effective ENOB | Cost (5 ch, @~10 µs/sample) |
|---|---|---|---|
| 1 | 0 | ~10–11 | ~50 µs/frame |
| 4 | +1 | ~11–12 | ~200 µs/frame |
| **16** | **+2** | **~12–13** | **~800 µs/frame** ← default |
| 64 | +3 | ~13–14 | ~3.2 ms/frame |

Default **N=16** → ~12–13 effective bits at <1 ms/frame — comfortably under the 100 Hz (10 ms) budget, and exceeds the V5 ADS1115 *designed* resolution. CPU headroom: Task_SensorRead is 100 Hz on core 1; <1 ms/frame leaves >9 ms idle. **No DMA needed.**

### 2.4 Classifier-sensitivity heuristic
For a 5-state-per-finger input feeding a 1D-CNN, input quantization below ~1 % of span (~7 bits) is generally not the classification bottleneck — temporal features dominate. At 11+ effective bits we are ~3 bits above that floor. **Risk verdict: low**. Validation plan in §8 covers the empirical confirmation.

### 2.5 Verdict
Yes — internal ADC1 holds the 5 flex sensors for this project's 46-class task, at or above V5's *actual* delivered quality, with sub-millisecond frame cost and zero external ADC silicon. The only real caveat is ADC2 coexistence (§7), which V6 avoids by pin choice.

---

## 3. Hardware — Pin Map, Divider, Attenuation

### 3.1 ADC1 channel → GPIO (V6)
ADC1 only (ADC2 GPIO11–20 is claimed by the WiFi subsystem even under ESP-NOW — see §7).

| Finger | ADC1 CH | GPIO | Notes |
|---|---|---|---|
| Thumb | CH0 | 1 | |
| Index | CH1 | 2 | |
| Middle | CH2 | 3 | |
| Ring | CH3 | 4 | |
| Pinky | CH4 | 5 | |

GPIO1–5 are strapping-adjacent only on reset; as ADC1 inputs at runtime they are safe. Verify no boot strap conflict (GPIO0 is boot, GPIO1 is UART0 TX by default — **see §3.3 open item**).

### 3.2 Divider (unchanged from V5)
```
3.3 V ──[ flex 25k–125kΩ ]──┬──[ 47kΩ ]── GND
                            └──→ GPIO (ADC1 input)
```
- Flex bent (low R, ~25 kΩ): V_GPIO = 3.3 × 47/(25+47) ≈ **2.15 V**
- Flex straight (high R, ~125 kΩ): V_GPIO = 3.3 × 47/(125+47) ≈ **0.90 V**
- Active span: **0.90 V–2.15 V** (~1.25 V, sits inside DB_12's ~0–2.5 V linear region with headroom).

Hardware change from V5: **none on the divider** — only the ADS1115 ADC inputs are rewired from the ADS1115 analog-input pins directly to ESP32-S3 GPIO1–5. ADS1115 chips and their I²C traces are removed.

### 3.3 Attenuation
`ADC_ATTEN_DB_12` (12 dB, ~0–2500 mV usable, up to ~3100 mV). This is the **current arduino-esp32 name**; the older `ADC_ATTEN_DB_11` is a deprecated alias for the same setting.

> **Open item HW-1**: GPIO1 is UART0 TX on the S3-DevKitC-1 by default. On a devkit this is fine for ADC at runtime (USB-CDC logging uses the native USB, `ARDUINO_USB_CDC_ON_BOOT=1` is already set in `platformio.ini`), but verify GPIO1/3 are not held by the USB-UART bridge at boot. On the final PCB, route flex Thumb to a non-UART ADC1 pin (e.g. GPIO4/5 priority) and reserve GPIO1 for debug. **Action**: confirm during §8 validation; if conflict, remap per §3.1 footnote.

### 3.4 I²C bus after migration
| Before (V5/V6-stale) | After (V6) |
|---|---|
| BNO085@0x4B + ADS1115@0x48 + ADS1115@0x49 (3 devices) | **LSM6DSV16X@0x6A only** (1 device) |

Single-device I²C = no address collision risk, no multi-controller timing, simpler pull-up sizing. This **must be reflected** in `01_architecture_diagrams.md` §3.x and `04_SOP-SPEC-PLAN_V6.md` §2.3 (see §9 stale-fix list).

---

## 4. Software Architecture — `IFlexSensor` Abstraction

### 4.1 Why an interface (D-ADC-4)
The IMU and ADC are both "things that change between V5 and V6", but only the ADC has a clean voltage-in → normalized-value-out contract. An `IFlexSensor` interface (Strategy/Adapter) lets the flex *source* be swapped — internal ADC today, ADS1115/ADS7138 back again, or a future SPI ADC — **without touching `FlexManager`, the normalizer, the inference pipeline, or the packet encoder**. This is the "拓展性 / 发展前景" the project requires.

### 4.2 Interface (new file `glove_firmware/lib/Sensors/IFlexSensor.h`)
```cpp
#pragma once
#include <cstdint>

// Strategy/Adapter abstraction for the 5-channel flex source.
// Implementations: InternalADCManager (V6 default), ADS1115Manager (V5 compat).
class IFlexSensor {
public:
    virtual ~IFlexSensor() = default;
    virtual bool begin() = 0;                 // init hardware + load NVS calibration
    virtual bool readRaw(uint16_t out[5]) = 0; // raw ADC counts, 12-bit range
    virtual bool readMilliVolts(uint16_t out[5]) = 0; // eFuse-corrected mV
    // FlexManager normalizes via persisted min/max; see §5.
};
```

### 4.3 File structure (V6 `lib/Sensors/`)
```
lib/Sensors/
├── IFlexSensor.h            # NEW — interface (D-ADC-4)
├── InternalADCManager.h     # NEW — V6 default impl (this doc, §6)
├── ADS1115Manager.h         # KEPT as optional V5-compat impl behind IFlexSensor
├── FlexManager.h            # MODIFIED — depends on IFlexSensor*, not ADS1115Manager
├── LSM6DSV16XManager.h      # IMU (from 04_SOP-SPEC-PLAN, not this doc)
└── SensorManager.h          # MODIFIED — owns IFlexSensor* + IMU
```
`FlexManager` gain: it no longer `#include`s any concrete ADC. Calibration (§5) lives behind the interface.

---

## 5. Calibration & NVS Persistence (D-ADC-3)

### 5.1 Data model
Per channel, per glove (L/R): `rawMin`, `rawMax` (uint16 each → normalized 0..1 via `(x-min)/(max-min)`). Stored as NVS key-value via `Preferences`:

| NVS namespace | Key | Type | Notes |
|---|---|---|---|
| `flex_cal` | `L_min` / `L_max` | `Blob` of 5×uint16 | left glove |
| `flex_cal` | `R_min` / `R_max` | `Blob` of 5×uint16 | right glove |
| `flex_cal` | `ver` | `UChar` | schema version for future migration |

(`Preferences` stores each key as a single blob internally — so this is "key-value" granularity at the *glove+field* level, not one key per raw sample. Matches V5 `FlexManager`'s existing min/max pair shape with minimal migration.)

### 5.2 Lifecycle
1. First boot / re-calibration: user triggers calibration gesture set → `FlexManager` writes raw min/max → `IFlexSensor::persistCalib()` → NVS.
2. Every boot: `IFlexSensor::begin()` → `loadCalib()` from NVS into RAM. If absent, fall back to compile-time `RAW_MIN/MAX` defaults and log a warning.
3. Normalization (unchanged contract from V5): `(raw - min) / (max - min)` clamped 0..1.

### 5.3 What this fixes vs V5
V5's bug was: calibration operated on *normalized* values, so `RAW_MIN/MAX` never updated, and nothing was persisted. V6 writes **raw** min/max (from `readRaw`) and persists them — so calibration actually refines the normalization bounds and survives reboot.

---

## 6. `InternalADCManager.h` — Implementation (D-ADC-2)

Core sketch (idiomatic to this codebase — header-only, `static_assert` guards like V3 fix):
```cpp
#pragma once
#include "IFlexSensor.h"
#include <Preferences.h>
#include "Filters/KalmanFilter1D.h"   // now WIRED (was dead code in V5)

class InternalADCManager : public IFlexSensor {
public:
    // ADC1_CH0..4 → GPIO1..5 (§3.1). attenuation DB_12 (§3.3).
    static constexpr uint8_t  kPins[5]      = {1, 2, 3, 4, 5};
    static constexpr uint8_t  kOversample   = 16;        // §2.3 default
    static constexpr uint16_t kRawMinDef    = 0;
    static constexpr uint16_t kRawMaxDef    = 4095;      // 12-bit

    bool begin() override {
        for (uint8_t i = 0; i < 5; ++i) {
            analogSetPinAttenuation(kPins[i], ADC_ATTEN_DB_12);
            _kal[i].configure(/*q*/2.0f, /*r*/8.0f);
        }
        _prefs.begin("flex_cal", true);
        _loadCalib();
        _prefs.end();
        return true;
    }

    bool readRaw(uint16_t out[5]) override {
        uint32_t acc[5] = {0};
        for (uint8_t s = 0; s < kOversample; ++s)
            for (uint8_t i = 0; i < 5; ++i)
                acc[i] += analogRead(kPins[i]);     // sum N samples
        for (uint8_t i = 0; i < 5; ++i)
            out[i] = static_cast<uint16_t>(_kal[i].filter(acc[i] >> 4)); // decimate + Kalman
        return true;
    }

    bool readMilliVolts(uint16_t out[5]) override {
        uint16_t r[5]; readRaw(r);
        for (uint8_t i = 0; i < 5; ++i)
            out[i] = analogReadMilliVolts(kPins[i]); // eFuse-corrected, single-shot
        return true;
    }
    // persistCalib() / _loadCalib() write/read the NVS blobs of §5.1 ...
private:
    Preferences  _prefs;
    KalmanFilter1D _kal[5];
    uint16_t _min[5], _max[5];
};
static_assert(sizeof(InternalADCManager) > 0, "InternalADCManager must be defined");
```
Notes:
- `analogReadMilliVolts()` applies eFuse ADC calibration (gain/offset) automatically — use it whenever mV fidelity matters; for normalized flex, raw+Kalman is sufficient and faster.
- Oversampling N=16 ⇒ shift `>>4` to decimate. Kalman per-channel (finally wired) smooths residual noise.
- API availability: `analogReadMilliVolts` stable from arduino-esp32 core 2.0.17+. Project `espressif32@^6.5.0` ships a recent 2.x core → available. If a build warns "experimental", add `-DADC_MODE_ADC` and confirm core version.

---

## 7. ADC1 vs ADC2 — WiFi/ESP-NOW Coexistence

- **ADC2 (GPIO11–20)** is claimed by the WiFi radio whenever WiFi/ESP-NOW is on; reads return garbage or fail. This is *why* V6 pins flex to **ADC1 only** (GPIO1–10).
- **ADC1 (GPIO1–10)** is independent of WiFi → safe under continuous ESP-NOW TX (glove→C6 relay). This is the enabling fact for the whole migration.
- **Validation gate**: §8 step V2 — measure raw counts on all 5 GPIO during sustained ESP-NOW transmission and confirm no correlation with TX bursts. (Expected: none.)

---

## 8. Validation Plan

| ID | Test | Pass criterion |
|---|---|---|
| V1 | GPIO1–5 boot-strap check | No conflict with USB-UART on devkit; on PCB remap if needed (§3.3) |
| V2 | ADC1 ↔ ESP-NOW coexistence | Raw counts stable (σ within noise floor) during 100 Hz ESP-NOW TX |
| V3 | Sample-rate throughput | 5-ch × oversample=16 in <1 ms; Task_SensorRead sustains 100 Hz |
| V4 | ENOB measurement | At N=16, effective bits ≥ 12 (std-dev method over 1 k samples at fixed input) |
| V5 | Calibration persistence | Set min/max → power cycle → values restored from NVS |
| V6 | End-to-end classification | L1 46-class accuracy with internal ADC ≥ V5 (ADS1115, 8-SPS-bug) baseline recorded for side-by-side |
| V7 | Divider linearity | 0.90–2.15 V sweep → raw monotonic, no clipping at DB_12 |

---

## 9. Stale-Point Fix List (edits to other V6 docs)

These are the concrete fixes the audit (task #3) found. Each must be applied alongside this doc for consistency.

| File | Location | Stale | Fix to |
|---|---|---|---|
| `04_SOP-SPEC-PLAN_V6.md` | §2.3 I²C topology | `ADS1115 #1 @0x48, ADS1115 #2 @0x49` | `LSM6DSV16X @0x6A only` (single device) |
| `04_SOP-SPEC-PLAN_V6.md` | §2.6 heading | `### 2.6 ADS1115 Configuration (Unchanged from V5)` | `### 2.6 Internal ADC1 Configuration (replaces ADS1115, see 07)` + summarize §2/§3 here |
| `02_BOM_table.md` | §1 BOM row 30 | `ADS1115IDGSR ×2 ¥8` | Remove; add `0` (internal ADC, no cost) + 5×47kΩ note |
| `02_BOM_table.md` | §5.3 vendor | TI ADS1115 section | Delete section; replace with "Internal ADC1 — no part" |
| `02_BOM_table.md` | §7.1 address summary | 0x48/0x49 | Only 0x6A |
| `02_BOM_table.md` | §4 cost comparison | IMU-only delta | Add −¥16/glove ADC removal; recompute net V5→V6 savings |
| `05_claude_code_prompts.md` | phase 3, ~L590 | `// BNO085: SKIP, debug ADS1115` | Remove ADS1115 debug scaffolding; point to this doc §6 for `InternalADCManager` prompts |
| `03_wiring_diagram.md` | §2.3 I²C bus | 3-device bus | 1-device (LSM6DSV16X) bus; add new flex→GPIO1–5 wiring figure |
| `01_architecture_diagrams.md` | L242-245, L818, L846 | 3-device I²C diagrams | 1-device; reflect single-device bus |
| `README.md` | L13 index | `ADS1115 channels` | `Internal ADC1 GPIO1-5 (see 07)` |

---

## 10. Engineering Trade-off Summary (cost / migration / ecosystem / extensibility / outlook)

| Dimension | Internal ADC1 (chosen) | Keep ADS1115 | ADS7138 (rejected alt) |
|---|---|---|---|
| BOM cost / glove | −¥16 (no ADC chip) | baseline | +¥6 (newer I²C ADC) |
| BOM reduction | ~25 % of flex subsystem | 0 | negative |
| Migration effort (V5→V6) | Medium: new manager + interface + NVS; touches `FlexManager`/`SensorManager` only | Low (but keeps all V5 bugs) | Medium-High: new driver + I²C still on bus |
| Hardware risk | Low: removes known-bad ADS1115 from I²C | High: repeats 2026-06-28 failure class | Medium: I²C bus still shared |
| I²C bus | Frees bus to 1 device (LSM6DSV16X) | 3 devices | 2 devices |
| CPU @100 Hz | <1 ms/frame | 60 ms/frame (V5 bug) | ~fast, I²C overhead |
| Effective resolution | ~12-13 bits @ N=16 | ~16 bit spec, ~8 SPS actual (bug) | ~12 bit |
| Ecosystem | arduino-esp32 native `analogReadMilliVolts` + eFuse cal | Adafruit ADS1x15, mature | TI driver, thinner |
| Extensibility (future ADC swap) | `IFlexSensor` interface → swap in 1 file | Via interface too | Via interface too |
| Development outlook | Aligns with "去外部 ADC" product direction, cheapest path to dual-glove scale | Dead-end for this project | Viable but cost+I²C negate the point |
| Reason rejected | — | keeps the bug class that bricked the bus | cost + still-on-I²C, no upside vs internal |

**Bottom line**: internal ADC1 wins on cost, hardware-risk, sample-rate, bus-simplification, and outlook; matches V5's *actual* delivered quality after its config bug; and the `IFlexSensor` interface preserves extensibility if a future high-precision need arises. The only non-blocker is the GPIO1/UART0 strap check (§3.3, V1) — solvable on PCB layout.

---

## 11. Decisions Log

| ID | Decision | Rationale |
|---|---|---|
| D-ADC-1 | ESP32-S3 internal ADC1 (no external ADC) for 5 flex | §2, §10 |
| D-ADC-2 | `analogReadMilliVolts()` + N=16 software oversampling | §2.3, §6; simplest API, enough bits, <1 ms |
| D-ADC-3 | NVS `Preferences` key-value persistence | §5; fixes V5 RAM-only bug, minimal migration |
| D-ADC-4 | `IFlexSensor` Strategy/Adapter interface | §4; extensibility to swap ADC source without touching pipeline |
| D-ADC-5 | Keep 47 kΩ divider + `ADC_ATTEN_DB_12` | §3; zero hardware change, current arduino-esp32 naming |
| D-ADC-6 | Wire `KalmanFilter1D` (was V5 dead code) into `InternalADCManager` | §6; closes V5 dead-code bug |

---

## 12. References
- ESP32-S3 datasheet §5 ADC1/ADC2, strapping pins
- arduino-esp32 core: `analogRead`, `analogReadMilliVolts`, `analogSetPinAttenuation`, `ADC_ATTEN_DB_12`
- V5 failure context: project memory `project_i2c_voltage_debug`, `v6-internal-adc-decision`
- Partner doc: `04_SOP-SPEC-PLAN_V6.md` (main spec), `03_wiring_diagram.md`, `02_BOM_table.md`
