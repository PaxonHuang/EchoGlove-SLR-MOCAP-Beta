# EchoGlove — Edge-AI Data Glove + P4 Smart Base Station

**3-tier inference system for real-time sign language translation and 3D hand animation, with dual-hand support.**

![EchoGlove](image/README/1778514913749.jpg)

> **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x` (active)
> **Last verified vs code**: 2026-07-11

---

## Competition Demo Fast-Path (2026-07-11, LIVE)

A lightweight, on-device demo path for the competition — bypasses the full V5/V6 relay + ST-GCN stack. **S3 flex → USB CDC → rule classifier → WebSocket → React**.

```
ESP32-S3 (5× flex, internal ADC1)  ──USB CDC "$EG,..."──▶  demo_server.py  ──WS:8765──▶  React+R3F
                                                              │
                                                              ▼
                                                   rule_classifier.py (8-sign euclidean
                                                   template matcher, 14 pytest pass)
```

**Demo features (all live)**:
- **Dashboard** (desktop): right-main ≥60% panel (fused result hero + sensor/stats 2-col), left 3D MOCAP corner ≤40%. Real-time **forward-kinematics** hand — `FingerChain` nested MCP→PIP→DIP rotation, palm + forearm, flex→15 joint angles. **Auto-speak English** on recognized gesture (confidence ≥ 0.6, debounced per new id; `GESTURE_LABELS_EN`).
- **Sign teaching page** (手语教学): full-screen enlarged anatomical hand + right button panel — **7 CSL signs** (你/好/再见/快乐/后悔/吃饭/睡觉) + **6 ASL letters** (A/B/I/L/W/Y). Click a button → plays keyframe animation **and** English TTS (CSL → `nameEn`, ASL → letter).
- **TTS**: zero-dependency Web Speech API (`useTTS` / `useGestureTTS` hooks), all English (en-US).
- **Classifier**: 4-channel A/B/I/L (ring ch3 hardware fault → dropped from active set; W/Y templates retained). Calibration via `glove_relay/scripts/calibrate_demo.py` (8-pose interactive capture, `demo_calibration.json`).

**Run the demo**:
```bash
# 1. Flash S3 (flex via internal ADC1, ESP-NOW intact, USB CDC "$EG,..." output every 3rd frame)
cd glove_firmware && pio run -t upload

# 2. Start demo relay (standalone, no main relay lifespan touched)
cd glove_relay
conda run -n pytorch_env python scripts/demo_server.py   # USB CDC→RuleClassifier→WS:8765

# 3. Web
cd glove_web && npm install && npm run dev               # http://localhost:5173
```

> udev: S3 on `/dev/ttyACM0` needs `99-platformio-udev.rules` (sudo pw `qwer` per local machine) — see memory `s3-serial-udev-perms`.

---

## System Status (code-verified 2026-07-11)

The project is mid-migration from V5 (BNO085 + ADS1115 + ESP-NOW) to V6 (LSM6DSV16X + internal ADC + wired UART / Wi-Fi UDP). To avoid misleading collaborators, the table below distinguishes **what actually runs** from **what is designed but not yet implemented**.

| Layer | Status | Detail |
|-------|--------|--------|
| ✅ Flex sensors (5×) | **Implemented** | ESP32-S3 **internal ADC1** (GPIO1–5), N=16 oversample, NVS calibration (`InternalADCManager.h`) |
| ✅ S3 → C6 comm | **Implemented** | ESP-NOW broadcast (`esp_now_send`, 69B `GlovePacket`) |
| ✅ C6 → P4 relay | **Implemented** | C6 receives ESP-NOW → UART 2 Mbps frame to P4 (`c6_firmware`) |
| ✅ P4 receive + output | **Implemented** | P4 UART RX (pin 37/38, 2 Mbps) → Tier2 stub → USB CDC JSON to PC |
| ✅ P4 standalone | **Verified** | LVGL display + TFLite stub + ES8311 audio + TinyUSB CDC init (`CONFIG_P4_INTERNAL_MOCK`) |
| 🟡 IMU (LSM6DSV16X) | **Designed / not implemented** | Driver does not exist in firmware yet; **IMU output is currently zeros** (`SensorManager.h` TODO). BNO085 path already removed. |
| 🟡 S3 → P4 wired UART | **Designed / not implemented** | `WIRED_UART` compile flag not in code; S3 currently uses ESP-NOW. Bypasses C6 over direct UART (GPIO6→GPIO38). See V5.3 wired spec. |
| 🟡 Wi-Fi / UDP (future) | **Planned** | On-board C6 is an ESP-Hosted Wi-Fi/BT co-processor (SDIO); will provide Wi-Fi/UDP in production, not ESP-NOW pass-through. |
| ❌ BNO085 / ADS1115 | **Deprecated** | BNO085 removed from active code (lib_deps entry is stale, see §Tech Debt); `ADS1115Manager.h` is dead code pending cleanup. |

> See `docs/V6/04_SOP-SPEC-PLAN_V6.md` for the full V6 design and `PROGRESS.md` for current checkpoints.

---

## Architecture

```
Current dev path (code-verified):
Gloves (ESP32-S3)        P4 Base Station            PC Relay             Frontend
┌─────────────┐ ESP-NOW  ┌──────┐ UART 2Mbps ┌──────────┐  USB CDC ┌──────────┐ WS:8765 ┌───────────┐
│ L/R Gloves  │──69B───→ │  C6  │───────────→│   P4     │─────────→│ FastAPI  │────────→│ React+R3F │
│ Tier1 CNN   │  (~2ms)  │relay │            │ Tier2    │  (JSON)  │ Tier3    │         │ 3D Hand   │
│ (flex+IMU*  │          └──────┘            │ LVGL+TTS │          │ ST-GCN   │         │ Skeleton  │
│  flex=ADC1) │                              └──────────┘          │ NLP+TTS  │         └───────────┘
└─────────────┘                                                    └──────────┘
Standalone mode (no PC): P4 runs Tier2 + LVGL display + TTS audio independently.

* IMU fields currently zero (LSM6DSV16X driver pending); flex via internal ADC1 works.

Planned (not yet implemented):
  S3 ──UART 2Mbps──► P4 (direct, bypass C6)   ← V5.3 "wired dev" path, designed
  C6 ──ESP-Hosted Wi-Fi/UDP──► P4             ← future production path
```

### Three-Tier Inference

| Tier | Location | Model | Latency | Classes |
|------|----------|-------|---------|---------|
| L1 (Edge) | ESP32-S3 glove | 1D-CNN+Attention | <3ms | 46 |
| L2 (P4 Base Station) | ESP32-P4 | Gated Bi-CrossAttention (TFLite Micro) | <30ms | 46 |
| L3 (PC) | Python relay | ST-GCN + MS-TCN | <50ms | 46+ |

### Core Constants (unchanged across V5→V6)

| Constant | Value |
|----------|-------|
| NUM_FLEX_SENSORS | 5 |
| SINGLE_HAND_FEATURES | 11 (5 flex + 3 euler + 3 gyro) |
| DUAL_HAND_FEATURES | 28 (L11 + R11 + Relative6) |
| GlovePacket size | 69 bytes |
| Sensor sampling rate | 100 Hz |

---

## Quick Start

**One-command environment setup** (Ubuntu x64, CPU-only): see `docs/DEVELOPMENT_SETUP.md`, or in Claude Code run `/setup-env`.

### 1. Glove Firmware (ESP32-S3, PlatformIO)

```bash
cd glove_firmware
pio run                    # Build
pio run -t upload          # Upload to ESP32-S3
pio device monitor         # Serial monitor (115200 baud)
pio test                   # Run firmware native tests
```

### 2. P4 Base Station (ESP-IDF v5.4+)

```bash
source scripts/activate_idf.sh                     # activate ESP-IDF (created by /setup-env)
cd glove_firmware/p4_base_station/p4_firmware
idf.py set-target esp32p4
idf.py build && idf.py -p /dev/ttyACM0 flash monitor   # P4 main processor

# Standalone verification (no C6/gloves needed):
idf.py -DIDF_TARGET=esp32p4 -DCONFIG_P4_INTERNAL_MOCK=1 build   # internal mock data
```

> **Note on C6**: The on-board ESP32-C6 is factory pre-flashed as an **ESP-Hosted Wi-Fi/BT co-processor** (SDIO). It does **not** support ESP-NOW pass-through. Do **not** flash the legacy `c6_firmware` mock-ESP-NOW bridge onto the EV board's C6 unless you intend to repurpose it. See `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`.

### 3. Python Relay (glove_relay)

```bash
conda run -n pytorch_env pip install -r glove_relay/requirements.txt  # or use environment.yml
conda run -n pytorch_env uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
conda run -n pytorch_env python -m pytest glove_relay/tests/    # Run relay tests
```

### 4. Web Frontend (glove_web)

```bash
cd glove_web
npm install
npm run dev                # Dev server: http://localhost:5173
npm run build              # Production build
```

**Views**:
- **仪表盘 (Dashboard)**: real-time 3D FK hand (R3F) + fused recognition panel + sensor/stats. Auto-plays English TTS on recognized gestures (confidence ≥ 0.6).
- **手语教学 (Sign Teaching)**: full-screen anatomical hand + button panel (7 CSL signs + 6 ASL letters). Click → animation + English TTS.

### 5. Unity Pro (glove_unity) — Windows Only

Unity 2022.3 LTS + XR Hands package. Open `glove_unity/` in Unity Hub (Windows).

---

## Hardware

### Gloves (ESP32-S3)

| Component | Spec | Status |
|-----------|------|--------|
| MCU | ESP32-S3-DevKitC-1 N16R8 (8MB Flash + 8MB PSRAM) | ✅ |
| IMU | LSM6DSV16X (6-axis, SFLP fusion) @ I2C 0x6A | 🟡 driver pending (IMU=zeros now) |
| Flex sensors | 5× Spectra Symbol 2.2" → **internal ADC1** (GPIO1–5) | ✅ |
| I2C | GPIO8 (SDA), GPIO9 (SCL), 400kHz — single device (LSM6DSV16X) | 🟡 |
| Communication | ESP-NOW broadcast (current); wired UART (designed, pending) | ✅ / 🟡 |
| **Removed (V6)** | ~~BNO085~~, ~~2× ADS1115~~, ~~TCA9548A MUX~~ | ❌ deprecated |

### P4 Smart Base Station

| Component | Spec | Notes |
|-----------|------|-------|
| Main MCU | ESP32-P4 (400MHz RV32, 32MB PSRAM) | ESP32-P4-Function-EV-Board v1.5.2 |
| Co-processor | ESP32-C6-MINI-1 | ESP-Hosted Wi-Fi/BT (factory FW), NOT an ESP-NOW relay |
| Display | 7" MIPI-DSI 1024×600 capacitive touch (GT911) | ✅ verified |
| Audio | ES8311 codec + NS4150 speaker (I2S, 16kHz/16-bit/mono) | ✅ init verified |
| USB | USB 2.0 HS OTG (480Mbps) — P4→PC CDC | ✅ |
| UART | S3/C6 → P4, 2Mbps, P4 GPIO37(TX)/GPIO38(RX) | ✅ |

---

## Tech Debt (pending cleanup — code, not docs)

Documented here per the "docs-only this round" decision (see `PROGRESS.md`):
- `platformio.ini` `lib_deps` still lists `Adafruit BNO08x` + `NimBLE-Arduino` — no active source includes them (stale deps).
- `glove_firmware/lib/Sensors/ADS1115Manager.h` exists but is unreferenced dead code.
- `Sensors.h` / `Comms.h` aggregate-header comments still describe V5 (ADS1115/BNO085).
- `data_structures.h` I2C comment "400kHz for 3 devices" is V5-stale (V6 = single device).

---

## Test Status

| Component | Tests | Status |
|-----------|-------|--------|
| Firmware (native, incl. V6 ADC) | 38 | ✅ |
| P4 base station (native) | 12 | ✅ |
| Relay (pytest) | varies (was 133→, see PROGRESS) | ✅ |
| Web | build-only | ✅ |

---

## Project Structure

```
glove_firmware/          # ESP32-S3 glove firmware (PlatformIO)
├── src/                 # FreeRTOS tasks + main
├── lib/                 # Sensors (InternalADCManager, IFlexSensor...), Models, Comms, Filters
├── shared/              # uart_frame.h, p4_protocol.h
├── p4_base_station/     # P4 + C6 base station
│   ├── c6_firmware/     # C6 ESP-NOW→UART relay (ESP-IDF) — see note above
│   ├── p4_firmware/     # P4 Tier2 + LVGL + TTS + USB CDC (ESP-IDF)
│   └── tests/           # Native tests
└── scripts/             # Model export, calibration, TTS gen
glove_relay/             # Python FastAPI relay server
glove_web/               # React + R3F 3D hand skeleton
glove_unity/             # Unity XR Hands (Windows)
scripts/setup_env.sh     # One-shot Ubuntu x64 dev environment bootstrap
docs/                    # Specs, plans, references (see docs/V6/, docs/superpowers/)
```

---

## Performance Targets

| Metric | Target |
|--------|--------|
| L1 inference latency | <3ms |
| L2 inference latency (P4) | <30ms |
| L3 inference latency (PC) | <50ms |
| End-to-end latency | <100ms |
| Sensor sampling rate | 100Hz |
| GlovePacket size | 69 bytes |
| Feature vector | 28-dim (L11 + R11 + Relative6) |

---

## Documentation

- **Dev environment**: `docs/DEVELOPMENT_SETUP.md` (one-command `/setup-env`)
- **V6 design spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md`
- **V6 internal ADC migration**: `docs/V6/07_internal_adc_migration.md`
- **Wiring diagrams**: `docs/V6/03_wiring_diagram.md`
- **Wired UART design (V5.3)**: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`
- **Cross-session progress**: `PROGRESS.md` (authoritative), `PROGRESS_CN.md` (summary)
- **Historical (V3–V5.2)**: `docs/archive/`

---

## License

Academic research project for national embedded chip/system design competition (2026).
