# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## V5.3 Wired Dev Path — S3→P4 Direct UART (2026-07-09, ACTIVE)

**Architecture pivot**: The on-board C6 on the P4 EV Board is an **ESP-Hosted Wi-Fi/BT co-processor** (SDIO bus, factory pre-flashed slave firmware v0.0.6). ESP-Hosted does **NOT support ESP-NOW** pass-through. The original `c6_firmware` mock-ESP-NOW bridge is incompatible with this hardware.

**Current dev path**: S3 gloves connect **directly** to the P4 over UART (2 Mbps, CRC-16/MODBUS framing via `shared/uart_frame.h`), bypassing C6 entirely. C6 is deferred to a future Wi-Fi integration phase.

- **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
- **Tag**: `v5.3-wired-dev` (baseline before UART implementation)
- **Design**: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`
- **Plan**: `docs/superpowers/plans/2026-07-09-s3-p4-wired-uart.md`
- **Wiring**: S3 GPIO6 (TX) → P4 GPIO38 (RX) + GND (see `docs/V6/03_wiring_diagram.md` §5)
- **S3 firmware**: ESP-NOW + UART parallel TX (compile flag `WIRED_UART=1`); UART is wired fallback
- **P4 firmware**: `uart_receiver` (UART0, GPIO37 TX / GPIO38 RX, 2Mbps) already complete; standalone verified via `CONFIG_P4_INTERNAL_MOCK=y` (commit `4f541bb`)
- **C6**: NOT flashed; stays as factory ESP-Hosted co-processor. Flash via PROG_C6 + CH340 only if a custom app is needed (P4 must be in bootloader mode first). See memory `p4-ev-board-c6-esp-hosted`.

**Verification status (2026-07-08)**: P4 standalone verified — LVGL display + TFLite stub + ES8311 audio init + TinyUSB CDC init all PASS. A3 hardware verification complete (commit `4f541bb`). Track B (mock relay→browser E2E) complete (commit `3cf2f3a`).

---

## V5.2 DualGloveFlex + P4 Base Station (2026-06-01)
- Branch: V5-DualGloveFlex
- V5 Spec: docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md
- V5.2 Spec: docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md
- Architecture: 2x ESP32-S3 gloves + C6 ESP-NOW relay + P4 smart base station (Tier2 + LVGL + TTS)
- Test Status (2026-06-22): 168/168 pass + BNO085 sensor data verified + ADS1115 I2C detection verified (0x48, 0x49)
- V5.2 P4 Base Station: 8/8 tasks done (commits f4e4d34..bef0c96), ready for hardware

### V5 Constants
| Constant | Value |
|----------|-------|
| NUM_FLEX_SENSORS | 5 |
| SINGLE_HAND_FEATURES | 11 (5 flex + 3 euler + 3 gyro) |
| DUAL_HAND_FEATURES | 28 (L11 + R11 + Relative6) |
| GlovePacket size | 69 bytes |
| ESP-NOW latency | ~2ms |
| I2C topology | Flat bus (no MUX): BNO085@0x4B + ADS1115@0x48 + ADS1115@0x49 |

---

## Workflow Rule: Execute First, Explore Second

When I provide detailed specs or explicit instructions for project initialization or implementation, start executing immediately. Only read/explore files if specs are ambiguous or missing critical information. Do not spend multiple rounds exploring existing structure before acting on clear instructions.

---

## Project Context

ESP32-S3 data glove + ESP32-P4 smart base station for hand sign recognition. Key components: BNO085 IMU, 5x flex sensors (via ADS1115), dual-hand support. 3-tier inference: L1 edge (S3 glove), L2 base station (P4 Tier2), L3 PC (ST-GCN). When continuing work, check `PROGRESS.md` before restarting from scratch.

---

## Project Overview

**Edge-AI Data Glove V5**: 3-tier inference system for real-time sign language translation and 3D hand animation, with dual-hand support.

**Architecture**:
- **Layer 1 (Edge)**: ESP32-S3 with 1D-CNN+Attention L1 model (<3ms latency)
- **Layer 2 (P4 Base Station)**: ESP32-P4 TFLite Micro Tier2 inference + LVGL display + TTS audio (standalone)
- **Layer 2 (PC Relay, fallback)**: Python FastAPI + ST-GCN + NLP + TTS
- **Layer 3a (Web MVP)**: React 18 + Vite + R3F (3D hand skeleton)
- **Layer 3b (Unity Pro)**: Unity 2022 LTS + XR Hands + ms-MANO

**Key Decisions**:
- **No Rust/Tauri**: V5 uses pure Web (React + R3F), no desktop framework
- **Python Relay**: Unified hub for UDP/USB→WebSocket conversion + L2 inference
- **Model Hot-Switch**: BaseModel interface + YAML config switching
- **BLE for Provisioning Only**: Frontend uses WiFi→Relay→WebSocket

---

## Directory Structure

```
├── glove_firmware/          # ESP32-S3 PlatformIO firmware (gloves)
│   ├── src/                 # Glove main.cpp + FreeRTOS tasks
│   ├── lib/                 # Sensors, Models, Comms, Filters
│   ├── shared/              # uart_frame.h, p4_protocol.h
│   ├── receiver/            # S3 USB receiver (FramePairer)
│   ├── p4_base_station/     # V5.2 P4 smart base station
│   │   ├── c6_firmware/     # C6 ESP-NOW relay (ESP-IDF)
│   │   ├── p4_firmware/     # P4 Tier2 inference + LVGL + TTS
│   │   └── tests/           # Native tests (pio test)
│   └── scripts/             # Model export, calibration, TTS gen
├── glove_relay/             # Python FastAPI relay server
├── glove_web/               # React + R3F frontend
├── glove_unity/             # Unity L3 Pro skeleton
├── docs/
│   ├── superpowers/         # V5/V5.2 design specs + implementation plans
│   ├── V5.0DualGloveFlex/   # V5.2 reference docs (GLM)
│   ├── archive/             # Historical docs (v3, v5-ai-drafts)
│   ├── references/          # PDF datasheets, research papers
│   └── notebooks/           # Jupyter notebooks (data, training)
└── CLAUDE.md
```

---

## Build Commands

### Firmware (glove_firmware)
```bash
cd glove_firmware
# Build
pio run

# Upload to ESP32-S3
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor

# Build with debug configuration
pio run -e esp32-s3-devkitc-1-n16r8_debug
```

### P4 Base Station (C6 + P4, ESP-IDF v5.4+)
```bash
# C6 co-processor
cd glove_firmware/p4_base_station/c6_firmware
idf.py set-target esp32c6
idf.py build && idf.py -p /dev/ttyUSBx flash monitor

# P4 main processor
cd glove_firmware/p4_base_station/p4_firmware
idf.py set-target esp32p4
idf.py build && idf.py -p /dev/ttyUSBx flash monitor
```

### Python Relay (glove_relay)
```bash
cd glove_relay
pip install -r requirements.txt
uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
```

### Web Frontend (glove_web)
```bash
cd glove_web
npm install
npm run dev                # Dev server http://localhost:5173
npm run build              # Production build
npm run preview            # Preview production build
```

**Vite Config:** `vite.config.ts` includes path alias `@/` pointing to `src/`

### Unity Pro (glove_unity) - Windows Only

**Platform**: Windows 10/11 with Unity 2022.3 LTS

Unity 开发环境保留在 **Windows** 平台（Linux 支持有限）。

```powershell
cd glove_unity
# Open in Unity Hub
# Build: File -> Build Settings -> PC, Mac & Linux Standalone
```

**Requirements:**
- Unity 2022.3 LTS or later
- Windows 10/11 with Visual Studio 2022
- XR Hands package (via Package Manager)

---

## Critical Bug Fix (FreeRTOS)

**V2 Bug**: `xTaskCreatePinnedToCore(TaskSensorReadHandle, ...)` — passed handle variable as function pointer

**V3 Fix**: `xTaskCreatePinnedToCore(Task_SensorRead, ...)` — correct function pointer + `static_assert` validation

---

## Cross-Platform Development (Windows + Ubuntu)

本项目支持 Windows 11 和 Ubuntu 24.04 协同开发，Line endings 由 `.gitattributes` 强制管理。

| 文件类型 | 行尾 | 说明 |
|---------|------|------|
| 源代码 (.c/.cpp/.h/.py/.ts/.js 等) | LF | 全平台统一 |
| Shell 脚本 (.sh/.bash) | LF | Linux 必须 |
| Windows 脚本 (.bat/.ps1/.cmd) | CRLF | Windows 专有 |

**关键规则**:
- `.claude/settings.local.json` 已 gitignore，各平台独立配置
- 修改 `.claude/settings.json` 时不要写平台特定路径
- Unity 开发保留在 Windows（Linux 支持有限）
- 固件构建前必须 `pio run` 验证，不可跳过

### Ubuntu 构建注意事项

ESP32 平台版本 `espressif32@^6.5.0`，新版编译器更严格：
- 类内部不能使用 `namespace`（须用 `struct`）
- Adafruit BNO08x v1.2.5 使用 `begin_I2C()` 而非 `begin()`
- `sh2_SensorValue_t` 使用 `.sensorId` 而非 `.type`
- 需显式 `#include <cfloat>` 才能使用 `FLT_MAX`

---

## FreeRTOS Task Architecture

| Task | Core | Priority | Frequency | Purpose |
|------|------|----------|-----------|---------|
| Task_SensorRead | 1 | 3 | 100Hz | I2C sampling + Kalman filter |
| Task_Inference | 0 | 2 | ~30Hz | L1 model inference |
| Task_Comms | 0 | 1 | 100Hz | BLE provisioning + UDP send |

---

## Data Flow

```
Gloves (S3)              P4 Base Station            PC Relay             Frontend
┌─────────────┐  ESP-NOW  ┌──────┐ UART 2Mbps ┌──────────┐  USB HS  ┌──────────┐  WS:8765  ┌───────────┐
│ L/R Gloves  │──~2ms──→│  C6  │───────────→│   P4     │────────→│ FastAPI  │────────→│ React+R3F │
│ Tier1 CNN   │           └──────┘           │ Tier2    │         │ Tier3    │         │ 3D Hand   │
│ 28-dim feat │                              │ LVGL+TTS │         │ ST-GCN   │         │ Skeleton  │
└─────────────┘                              └──────────┘         │ NLP+TTS  │         └───────────┘
                                                                  └──────────┘
Standalone mode (no PC): P4 runs Tier2 + LVGL display + TTS audio independently.
```

---

## Model Hot-Switch Architecture

- **BaseModel Interface**: Python (`glove_relay/src/models/base_model.py`) + C++ (`glove_firmware/lib/Models/BaseModel.h`)
- **Model Registry**: Dynamically load/switch models at runtime
- **YAML Config**: `glove_relay/configs/model_config.yaml` defines active model

---

## Key Files

| File | Purpose |
|------|---------|
| `glove_firmware/src/main.cpp` | FreeRTOS tasks with static_assert fix |
| `glove_firmware/lib/Models/ModelRegistry.h` | L1 model hot-switch |
| `glove_firmware/shared/uart_frame.h` | CRC-16/MODBUS UART frame encode/decode |
| `glove_firmware/p4_base_station/p4_firmware/main/main.cpp` | P4 FreeRTOS: UART, inference, display, audio, USB |
| `glove_firmware/p4_base_station/c6_firmware/main/main.cpp` | C6 ESP-NOW receive + UART relay |
| `glove_relay/src/main.py` | FastAPI + WebSocket relay |
| `glove_relay/src/models/stgcn_model.py` | L2 ST-GCN implementation |
| `glove_relay/src/usb_cdc_server.py` | P4 USB CDC serial input to relay |
| `glove_web/src/hooks/useWebSocket.ts` | WebSocket client with auto-reconnect |
| `glove_web/src/components/Hand3D/HandSkeleton.tsx` | 21-keypoint 3D hand |

---

## Hardware Context

- **Glove MCU**: ESP32-S3-DevKitC-1 N16R8 (8MB Flash + 8MB PSRAM)
- **Base Station**: ESP32-P4 (400MHz RV32, 32MB PSRAM) + ESP32-C6-MINI-1 co-processor
- **I2C**: GPIO 8 (SDA), GPIO 9 (SCL), 400kHz — flat bus: BNO085@0x4B + ADS1115@0x48 + ADS1115@0x49
- **Sensors**: BNO085 IMU (address 0x4B), 5x flex sensors (via 2x ADS1115 ADC)
- **V5 removed components**: TMAG5273 (Hall sensor), TCA9548A (I2C MUX) — see `docs/archive/v3/` for historical wiring

### BNO085 Wiring (ESP32-S3-DevKitC-1 N16R8)

| BNO085 Pin | ESP32-S3 Pin | Notes |
|------------|--------------|-------|
| VCC | 3.3V | **NOT 5V!** |
| GND | GND | |
| SDA | GPIO8 | 4.7kΩ pull-up to 3.3V |
| SCL | GPIO9 | 4.7kΩ pull-up to 3.3V |
| CS | 3.3V | HIGH for I2C mode |
| PS0 | GND | **GND=I2C, 3.3V=SPI** |
| PS1 | GND | Must be GND |
| ADO | 3.3V | HIGH=0x4B, LOW=0x4A |
| RST | GPIO10 | Optional: 3.3V if not needed |
| INT | Floating | Not used |

**Critical Notes:**
- PS0/PS1 are latched at power-up. PS0=3.3V selects SPI mode and can damage the module!
- GPIO8/9 works on N16R8 (unlike N8 variant where GPIO8/9 conflict with internal flash)
- Adafruit BNO08x v1.2.5 uses `begin_I2C()` not `begin()`
- `sh2_SensorValue_t` uses `.sensorId` not `.type`
- BNO085 needs power cycle for clean initialization (fails if in "hot" state)
- RST pin has strong internal pull-up — GPIO10 cannot pull LOW, use power cycle instead

---

## Performance Targets

| Metric | Target |
|--------|--------|
| L1 inference latency | <3ms |
| L2 inference latency | <20ms |
| End-to-end latency | <100ms |
| Sensor sampling rate | 100Hz |
| L1 accuracy (46 classes) | >90% Top-1 |
| L2 accuracy (46 classes) | >95% Top-1 |

---

## Development Phases

| Phase | Name | Status |
|-------|------|--------|
| P0 | Project init (PlatformIO + React + FastAPI) | Done |
| P1 | HAL & drivers (BNO085, ADS1115) | Done |
| P2 | Signal processing (Kalman filter, normalization, sliding window) | Done |
| P3 | L1 Edge Inference — Edge Impulse MVP (path A) | Done |
| P4 | Communication (BLE provisioning + WiFi UDP) | Done |
| P5 | Python Relay + L2 ST-GCN + NLP + TTS | Done (tests 88/88) |
| P6 | Web rendering (React + R3F) / Unity Pro | Done |
| V5 | DualGloveFlex migration (flex sensors, dual-hand, 3-tier) | Done |
| V5.2 | P4 Smart Base Station (C6 + P4 + LVGL + TTS) | **Done (code)** |

**Next**: Hardware testing — flash C6→verify ESP-NOW, flash P4→verify UART data flow. See `PROGRESS.md` for details.

---

## Documentation

- **V5 Design Spec**: `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md`
- **V5 Plan**: `docs/superpowers/plans/2026-06-01-v5-dual-glove-flex.md`
- **V5.2 P4 Design Spec**: `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`
- **V5.2 P4 Plan**: `docs/superpowers/plans/2026-06-10-v52-p4-base-station.md`
- **V5.2 Reference Docs**: `docs/V5.0DualGloveFlex/` (GLM generated)
- **Historical (V3/V4/V5 drafts)**: `docs/archive/`

---

## Skill Agents Available

- `esp32-firmware-engineer`: ESP-IDF/Arduino firmware, FreeRTOS
- `embedded-systems`: RTOS, memory optimization
- `feature-dev`: Guided feature development

Invoke with `/agent esp32-firmware-engineer` for firmware tasks.

---

## Project Dependencies

- BNO085 driver is a LOCAL driver included in the firmware repo, NOT a PlatformIO registry library. Do not search PlatformIO registry for it.
- For any dependency, check the project's existing `lib/` directory first before assuming it needs to be installed from a registry.

---

## Build Verification

After modifying any source file in this project, always run `pio run` to verify the build compiles successfully. Do not consider a task complete until a clean build is confirmed.

---

## PlatformIO Dependency Notes

- **lib_deps syntax**: Use `owner/libname @ version` (space before @), NOT `owner/libname=@version`
- **TFLite Micro**: Use `tanakamasayuki/TensorFlowLite_ESP32` (ESP32 optimized)

## Library Architecture

### Firmware (`glove_firmware/lib/`)
| Directory | Purpose |
|-----------|---------|
| `Sensors/` | BNO085 IMU driver, ADS1115 ADC, FlexSensorManager |
| `Models/` | BaseModel interface, TFLiteModel, ModelRegistry |
| `Comms/` | BLEManager, UDPTransmitter, Protobuf |
| `Filters/` | Kalman filter implementations |

### Relay (`glove_relay/src/`)
| Module | Purpose |
|--------|---------|
| `models/` | ST-GCN L2 inference, ModelRegistry, base_model interface |
| `nlp/` | CSL→Mandarin grammar correction |
| `tts/` | edge-tts voice synthesis |
| `utils/` | Config loading, logging |

### Web (`glove_web/src/`)
| Directory | Purpose |
|-----------|---------|
| `components/Hand3D/` | 21-keypoint hand skeleton (R3F) |
| `hooks/` | useWebSocket with auto-reconnect |
| `stores/` | Zustand state management |
| `types/` | TypeScript type definitions |

---

## MCP Plugins

| Plugin | Status | Usage |
|--------|--------|-------|
| Playwright | Working | Browser automation, web testing |
| Chrome DevTools | Working | Page inspection, performance profiling |
| GitHub MCP | Working (after restart) | PRs, issues, code search — needs `GITHUB_PERSONAL_ACCESS_TOKEN` in `.claude/settings.local.json` |
| Context7 | Intermittent | Library docs lookup — may fail due to proxy routing |
| Espressif Docs | Intermittent | ESP-IDF docs — may fail due to proxy routing |

Context7 and Espressif Docs failures are proxy-related (`127.0.0.1:15721`), not configuration issues. They may work intermittently.

---

## Session Continuation

**Read `PROGRESS.md` first** when starting a new session. It tracks cross-session state, completed checkpoints, and MCP status. Continue from the last checkpoint — do not re-explore or re-plan what's already done. Update `PROGRESS.md` when you finish a task.

---

## Gotchas

- Session continuation `.txt` files in project root are ephemeral — can be deleted
- **WebSocket Port**: Relay uses port **8765** for WebSocket (not 8000)
- **UDP Port**: ESP32 sends to port **8888** (configured in `platformio.ini`)
- **Relay Host**: Web frontend connects to `ws://${relayHost}:8765` — default is `localhost`
- **File line endings**: Managed by `.gitattributes` — LF for all source, CRLF only for Windows scripts
- **P4 UART**: C6→P4 uses 2Mbps UART with CRC-16/MODBUS. Frame: `[0xAA 0x55 69-byte payload CRC16_L CRC16_H]` = 73 bytes
- **P4 vs S3 builds**: P4/C6 use ESP-IDF (`idf.py`), S3 gloves use PlatformIO (`pio`) — different build systems
- **BNO085 PS0/PS1**: PS0/PS1 are latched at power-up. PS0=3.3V selects SPI mode and can damage the module! Always connect PS0=GND for I2C mode.
- **BNO085 RST**: RST pin has strong internal pull-up. GPIO10 cannot pull LOW. Use power cycle (disconnect VCC 10s) to reset.
- **BNO085 init**: Adafruit library fails if BNO085 is in "hot" state. Always power cycle before initializing.
- **BNO085 GPIO8/9**: Works on N16R8 (unlike N8 variant where GPIO8/9 conflict with internal flash)

## Testing

### Firmware Tests (glove)
```bash
cd glove_firmware
pio test
```

### P4 Base Station Tests (native)
```bash
cd glove_firmware/p4_base_station/tests
pio test
```

### Relay Tests
```bash
cd glove_relay
python -m pytest tests/
```