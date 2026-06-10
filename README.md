# EchoGlove V5.2 — Edge-AI Data Glove + P4 Smart Base Station

**3-tier inference system for real-time sign language translation and 3D hand animation, with dual-hand support.**

![1778514913749](image/README/1778514913749.jpg)

---

## Architecture

```
Gloves (ESP32-S3)         P4 Base Station           PC Relay           Frontend
┌─────────────┐  ESP-NOW  ┌──────┐ UART 2Mbps ┌──────────┐  USB HS  ┌──────────┐  WS:8765  ┌───────────┐
│ L/R Gloves  │──~2ms──→ │  C6  │───────────→│   P4     │────────→│ FastAPI  │────────→│ React+R3F │
│ Tier1 CNN   │           └──────┘           │ Tier2    │         │ Tier3    │         │ 3D Hand   │
│ 28-dim feat │                              │ LVGL+TTS │         │ ST-GCN   │         │ Skeleton  │
└─────────────┘                              └──────────┘         │ NLP+TTS  │         └───────────┘
                                                                  └──────────┘
Standalone mode (no PC): P4 runs Tier2 + LVGL display + TTS audio independently.
```

### Three-Tier Inference

| Tier | Location | Model | Latency | Classes | Accuracy |
|------|----------|-------|---------|---------|----------|
| L1 (Edge) | ESP32-S3 glove | 1D-CNN+Attention | <3ms | ~20 | ~85% |
| L2 (P4 Base Station) | ESP32-P4 | GatedBiCrossAttention | <30ms | 46 | ~90% |
| L3 (PC) | Python relay | ST-GCN + MS-TCN | <50ms | 60+ | ~95% |

### V5.2 P4 Smart Base Station (NEW)

- **C6 co-processor**: ESP-NOW relay from gloves → UART 2Mbps to P4
- **P4 main processor**: Tier2 inference (TFLite Micro, ~80KB INT8) + 7" MIPI-DSI LVGL touchscreen + ES8311 TTS audio + USB 2.0 HS
- **Standalone mode**: Works fully without PC — inference + display + audio on P4

---

## Quick Start

### 1. Glove Firmware (ESP32-S3, PlatformIO)

```bash
cd glove_firmware
pio run                    # Build
pio run -t upload          # Upload to ESP32-S3
pio device monitor         # Serial monitor (115200 baud)
pio test                   # Run firmware tests
```

### 2. P4 Base Station (ESP-IDF v5.4+)

```bash
# C6 co-processor
cd glove_firmware/p4_base_station/c6_firmware
idf.py set-target esp32c6
idf.py build && idf.py -p /dev/ttyUSBx flash monitor

# P4 main processor
cd glove_firmware/p4_base_station/p4_firmware
idf.py set-target esp32p4
idf.py build && idf.py -p /dev/ttyUSBx flash monitor

# P4 native tests
cd glove_firmware/p4_base_station/tests
pio test
```

### 3. Python Relay (glove_relay)

```bash
cd glove_relay
pip install -r requirements.txt
uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
python -m pytest tests/    # Run relay tests
```

### 4. Web Frontend (glove_web)

```bash
cd glove_web
npm install
npm run dev                # Dev server: http://localhost:5173
npm run build              # Production build
```

### 5. Unity Pro (glove_unity) — Windows Only

Unity 2022.3 LTS + XR Hands package. See `glove_unity/README.md`.

---

## Hardware

### Gloves (ESP32-S3)

| Component | Spec |
|-----------|------|
| MCU | ESP32-S3-DevKitC-1 N16R8 (8MB Flash + 8MB PSRAM) |
| IMU | BNO085 (9-axis, address 0x4B) |
| Flex sensors | 5x Spectra Symbol 2.2" (via 2x ADS1115 ADC) |
| I2C | GPIO 8 (SDA), GPIO 9 (SCL), 400kHz, flat bus |
| ADC addresses | ADS1115 #1: 0x48, ADS1115 #2: 0x49 |
| Communication | ESP-NOW (~2ms latency) |

### P4 Smart Base Station

| Component | Spec |
|-----------|------|
| Main MCU | ESP32-P4 (400MHz RV32 dual-core, 32MB PSRAM) |
| Co-processor | ESP32-C6-MINI-1 (ESP-NOW + BLE 5.0) |
| Display | 7" MIPI-DSI 1024x600 capacitive touch (GT911) |
| Camera | OV2710 2MP MIPI-CSI (deferred) |
| Audio | ES8311 codec + NS4150 speaker (I2S, 16kHz, 16-bit, mono) |
| USB | USB 2.0 HS OTG (480Mbps) |
| UART | C6→P4, 2Mbps, GPIO43→GPIO38 |

---

## Key Design Decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Flex sensors (V5) | Spectra Symbol 2.2" + ADS1115 | Mature, proven SLR approach |
| Communication | ESP-NOW | ~2ms latency, no WiFi/BLE overhead |
| Base station | ESP32-P4 + C6 | Competition board, standalone Tier2 inference |
| L2 model | GatedBiCrossAttention | Dual-hand cross-attention for sign language |
| Frontend | React + R3F (no Tauri/Rust) | Pure web, zero-install |
| Model hot-switch | BaseModel + YAML config | Runtime switching without restart |
| BLE | Provisioning only | No Web Bluetooth API (unstable) |

---

## Test Status

| Component | Tests | Status |
|-----------|-------|--------|
| Firmware (native) | 64 | ✅ |
| Receiver (native) | 12 | ✅ |
| P4 base station (native) | 12 | ✅ |
| Relay (pytest) | 80 | ✅ |
| **Total** | **168** | **All pass** |

---

## Project Structure

```
glove_firmware/          # ESP32-S3 glove firmware (PlatformIO)
├── src/                 # FreeRTOS tasks + main
├── lib/                 # Sensors, Models, Comms, Filters
├── shared/              # uart_frame.h, p4_protocol.h
├── receiver/            # S3 USB receiver (FramePairer)
├── p4_base_station/     # V5.2 P4 smart base station
│   ├── c6_firmware/     # C6 ESP-NOW relay (ESP-IDF)
│   ├── p4_firmware/     # P4 Tier2 + LVGL + TTS + USB
│   └── tests/           # Native tests
└── scripts/             # Model export, calibration, TTS gen

glove_relay/             # Python FastAPI relay server
glove_web/               # React + R3F 3D hand skeleton
glove_unity/             # Unity XR Hands (Windows)
docs/                    # Specs, plans, references
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
| ESP-NOW latency | ~2ms |
| GlovePacket size | 69 bytes |
| Feature vector | 28-dim (L11 + R11 + Relative6) |

---

## Documentation

- **V5 Design Spec**: `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md`
- **V5.2 P4 Design Spec**: `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`
- **Hardware Assembly**: `docs/HARDWARE_ASSEMBLY_GUIDE.md`
- **Wiring Debug Guide**: `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`
- **Research Papers**: `docs/references/`

---

## License

Academic research project for national embedded chip/system design competition (2026).
