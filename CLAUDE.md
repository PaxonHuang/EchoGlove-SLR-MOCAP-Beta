# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Quick Environment Setup (Ubuntu x64, CPU-only)

**One command in Claude Code → `/setup-env`** (or `./scripts/setup_env.sh` in shell). Idempotent, detect-first: only installs missing tools, never clobbers existing installs. Stages: base-os / conda / node / platformio / esp-idf / relay / web / verify.

- Full guide: `docs/DEVELOPMENT_SETUP.md`
- Conda envs (declarative): `glove_relay/environment.yml` (`pytorch_env`) + `glove_relay/environment_tf.yml` (`tf_env`), both CPU-only
- ESP-IDF activate per-shell: `source scripts/activate_idf.sh` (IDF v5.4, P4 BSP requirement)
- Lightweight shortcut (skip the ~2GB IDF): `./scripts/setup_env.sh --relay --web`
- **New collaborators**: after clone, run `/setup-env`, then read `PROGRESS.md` for current state. Personal tokens go in gitignored `.claude/settings.local.json`.

## V5.3 Wired Dev Path — S3→P4 Direct UART (2026-07-09, ACTIVE)

**Architecture pivot**: The on-board C6 on the P4 EV Board is an **ESP-Hosted Wi-Fi/BT co-processor** (SDIO bus, factory pre-flashed slave firmware v0.0.6). ESP-Hosted does **NOT support ESP-NOW** pass-through. The original `c6_firmware` mock-ESP-NOW bridge is incompatible with this hardware.

**Current dev path**: S3 gloves connect **directly** to the P4 over UART (2 Mbps, CRC-16/MODBUS framing via `shared/uart_frame.h`), bypassing C6 entirely. C6 is deferred to a future Wi-Fi integration phase.

- **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
- **Tag**: `v5.3-wired-dev` (baseline before UART implementation)
- **Design**: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`
- **Plan**: `docs/superpowers/plans/2026-07-09-s3-p4-wired-uart.md`
- **Wiring**: S3 GPIO6 (TX) → P4 GPIO38 (RX) + GND (see `docs/V6/03_wiring_diagram.md` §5)
- **S3 firmware (current, code-verified 2026-07-10)**: ESP-NOW broadcast (`esp_now_send`, 69B GlovePacket) is the active comm path. The wired UART (`WIRED_UART`) path is **designed but not yet implemented** — the compile flag does not exist in code yet. See `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`.
- **IMU (current)**: **zeros**. BNO085 path removed from active code; LSM6DSV16X driver does **not exist yet** (`SensorManager.h` TODO). Only Flex internal-ADC migration has landed.
- **P4 firmware**: `uart_receiver` (UART0, GPIO37 TX / GPIO38 RX, 2Mbps) receives from C6; output via USB CDC. Standalone verified via `CONFIG_P4_INTERNAL_MOCK=y` (commit `4f541bb`)
- **C6**: NOT flashed; stays as factory ESP-Hosted co-processor. Flash via PROG_C6 + CH340 only if a custom app is needed (P4 must be in bootloader mode first). See memory `p4-ev-board-c6-esp-hosted`.

**Verification status (2026-07-08)**: P4 standalone verified — LVGL display + TFLite stub + ES8311 audio init + TinyUSB CDC init all PASS. A3 hardware verification complete (commit `4f541bb`). Track B (mock relay→browser E2E) complete (commit `3cf2f3a`).

---

## V5.2 DualGloveFlex + P4 Base Station (2026-06-01, historical)
> **Historical reference** — V5/V5.2 used BNO085 + ADS1115 + ESP-NOW→C6→UART→P4. V6 migration supersedes the sensor/ADC/comm picks; see V6 docs + System Status. Design specs retained under `docs/superpowers/` for history.
- Branch: V5-DualGloveFlex (historical)
- V5 Spec: docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md
- V5.2 Spec: docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md
- V5.2 P4 Base Station: 8/8 tasks code-complete (commits f4e4d34..bef0c96)

### V5/V6 Shared Constants (unchanged across migration)
| Constant | Value | V6 status |
|----------|-------|-----------|
| NUM_FLEX_SENSORS | 5 | ✅ via internal ADC1 |
| SINGLE_HAND_FEATURES | 11 (5 flex + 3 euler + 3 gyro) | ✅ flex; euler/gyro=0 (IMU pending) |
| DUAL_HAND_FEATURES | 28 (L11 + R11 + Relative6) | ✅ (structure) |
| GlovePacket size | 69 bytes | ✅ |
| ESP-NOW latency | ~2ms | ✅ (current S3 comm) |
| I2C topology | V6: single device LSM6DSV16X@0x6A (GPIO8/9, 400kHz) | 🟡 driver not implemented, no I2C active in build yet. (V5 was BNO085@0x4B + ADS1115@0x48/0x49, all deprecated) |

---

## Workflow Rule: Execute First, Explore Second

When I provide detailed specs or explicit instructions for project initialization or implementation, start executing immediately. Only read/explore files if specs are ambiguous or missing critical information. Do not spend multiple rounds exploring existing structure before acting on clear instructions.

---

## Project Context

ESP32-S3 data glove + ESP32-P4 smart base station for hand sign recognition. **Current components (V6, code-verified 2026-07-10)**: 5x flex sensors via ESP32-S3 internal ADC1 (implemented); IMU = LSM6DSV16X (designed, **driver not implemented — IMU output zeros**); comm = ESP-NOW broadcast (implemented). BNO085/ADS1115 deprecated (dead code/lib_deps). 3-tier inference: L1 edge (S3 glove), L2 base station (P4 Tier2), L3 PC (ST-GCN). When continuing work, check `PROGRESS.md` before restarting from scratch.

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
- **Python Relay**: Unified hub for P4 USB CDC / historical UDP → WebSocket conversion + L2/3 inference
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
│   ├── superpowers/         # V5/V5.2/V6 design specs + implementation plans
│   ├── V6/                  # V6 LSM6DSV16X + internal-ADC design package (01–07)
│   ├── archive/             # Historical docs (v3, v5-ai-drafts, V5.0DualGloveFlex, AGENTS, ONBOARDING)
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
| Task_SensorRead | 1 | 3 | 100Hz | ADC1 flex sampling + Kalman filter (IMU=zeros, LSM6DSV16X pending) |
| Task_Inference | 0 | 2 | ~30Hz | L1 model inference |
| Task_Comms | 0 | 1 | 100Hz | ESP-NOW broadcast (UDP send is V5-historical, not active) |

---

## Data Flow (current, code-verified 2026-07-10)

```
Gloves (S3)              P4 Base Station            PC Relay             Frontend
┌─────────────┐  ESP-NOW  ┌──────┐ UART 2Mbps ┌──────────┐  USB CDC ┌──────────┐  WS:8765  ┌───────────┐
│ L/R Gloves  │──69B───→│  C6  │───────────→│   P4     │─────────→│ FastAPI  │────────→│ React+R3F │
│ Tier1 CNN   │  (current│relay │ (73B frame)│ Tier2    │  (JSON)  │ Tier3    │         │ 3D Hand   │
│ flex=ADC1✅ │  comm)   └──────┘            │ LVGL+TTS │          │ ST-GCN   │         │ Skeleton  │
│ IMU =zero 🟡│                              └──────────┘          │ NLP+TTS  │         └───────────┘
└─────────────┘                                                    └──────────┘
Standalone mode (no PC): P4 runs Tier2 + LVGL display + TTS audio independently.

Planned (NOT yet in code):
  S3 ──direct UART 2Mbps──► P4   (bypass C6; "V5.3 wired dev" path — WIRED_UART flag not implemented)
  C6 ──ESP-Hosted Wi-Fi/UDP──► P4  (future production)
```

**Note**: The C6→P4 UART relay path above is the designed chain, but the on-board C6 is an ESP-Hosted co-processor that **cannot run** the `c6_firmware` ESP-NOW bridge. Until the wired-UART (S3→P4 direct) path is implemented in firmware, the only end-to-end-verified path is **P4 standalone with internal mock data** (`CONFIG_P4_INTERNAL_MOCK=y`).

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

## Hardware Context (V6, code-verified 2026-07-10)

- **Glove MCU**: ESP32-S3-DevKitC-1 N16R8 (8MB Flash + 8MB PSRAM)
- **Base Station**: ESP32-P4 (400MHz RV32, 32MB PSRAM) + ESP32-C6-MINI-1 (ESP-Hosted co-processor)
- **IMU**: LSM6DSV16X @ I2C 0x6A (GPIO8 SDA / GPIO9 SCL, 400kHz) — 🟡 **driver not implemented**, IMU output currently zeros
- **Flex**: 5× flex sensors → **ESP32-S3 internal ADC1** (GPIO1–5), N=16 oversample, NVS calibration ✅
- **Comm**: S3 ESP-NOW broadcast (current, ✅); wired UART S3→P4 GPIO6→GPIO38 (designed, 🟡 not implemented)
- **Deprecated**: ~~BNO085~~, ~~2× ADS1115 (0x48/0x49)~~, ~~TCA9548A MUX~~, ~~TMAG5273~~ — see `docs/archive/`

### LSM6DSV16X Wiring (breakout module, I²C — V6 target, see `docs/V6/03_wiring_diagram.md` §2)

Project uses a **breakout module** (LGA-14L on small PCB; merges VDD/VDDIO→VCC, has decoupling caps). The chip has **no driver yet** (IMU=zeros).

| Breakout Pin | ESP32-S3 Pin | Notes |
|--------------|--------------|-------|
| VCC | 3.3V | **NOT 5V** (1.71–3.6V) |
| GND | GND | |
| ADO/MISO (SA0) | GND | LOW=0x6A (HIGH=0x6B) |
| SDA/MOSI | GPIO8 | 4.7kΩ pull-up |
| SCL/SCLK | GPIO9 | 4.7kΩ pull-up |
| CS | **3.3V** | **mandatory HIGH for I²C** (LOW→SPI, no I²C response) |
| INT1 | GPIO10 (optional) | NC if polling |
| **SDX / SCX** | **NC** | aux sensor-hub I²C bus — **do not wire** (common wiring mistake) |
| INT2 | NC (optional) | |

> Bare LGA-14L pad map (custom PCB) in `03_wiring_diagram.md` §2.2.
> **BNO085** historically retained in `03_wiring_diagram.md` §8.4 (migration reference); no longer in active code.

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

## Development Phases (V5/V6 migration timeline)

| Phase | Name | Status |
|-------|------|--------|
| P0–P6 | V5 pipeline (HAL, signal, L1, comm, relay, web) | Done (historical) |
| V5 | DualGloveFlex (flex sensors, dual-hand, 3-tier) | Done |
| V5.2 | P4 Smart Base Station (C6 + P4 + LVGL + TTS) | **Done (code)** |
| V6.0 | LSM6DSV16X + internal-ADC migration | 🟡 In progress (see below) |

**V6 implementation status (code-verified 2026-07-10)**:
- ✅ Flex: internal ADC1 (GPIO1–5) + `IFlexSensor` + NVS calibration — landed
- ✅ S3 ESP-NOW broadcast, P4 UART RX + USB CDC, P4 standalone mock-verified
- 🟡 LSM6DSV16X driver — **not implemented** (IMU=zeros); BNO085 removed from active code
- 🟡 Wired UART (S3→P4 direct) — designed (`WIRED_UART` flag not in code yet)
- ❌ Deprecated: BNO085, ADS1115 (dead code), TMAG5273, TCA9548A

**Next**: implement LSM6DSV16X driver → implement wired UART path → on-device verification. See `PROGRESS.md` for checkpoints + tech-debt cleanup list.

---

## Documentation

- **V5 Design Spec**: `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md`
- **V5 Plan**: `docs/superpowers/plans/2026-06-01-v5-dual-glove-flex.md`
- **V5.2 P4 Design Spec**: `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md`
- **V5.2 P4 Plan**: `docs/superpowers/plans/2026-06-10-v52-p4-base-station.md`
- **V5.2 Reference Docs (GLM)**: `docs/archive/v5-ai-drafts/V5.0DualGloveFlex/` (archived, non-authoritative)
- **Historical (V3/V4/V5 drafts)**: `docs/archive/`

---

## Skill Agents Available

- `esp32-firmware-engineer`: ESP-IDF/Arduino firmware, FreeRTOS
- `embedded-systems`: RTOS, memory optimization
- `feature-dev`: Guided feature development

Invoke with `/agent esp32-firmware-engineer` for firmware tasks.

---

## Project Dependencies

- BNO085 is deprecated. The V6 IMU (LSM6DSV16X) driver will be a LOCAL driver in the firmware repo, NOT a PlatformIO registry library. Check `lib/` first before searching the registry for any dependency.
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
| `Sensors/` | `InternalADCManager` (✅), `IFlexSensor`, `FlexManager`, `SensorManager` (IMU=zeros); legacy `ADS1115Manager.h` (dead code) |
| `Models/` | BaseModel interface, TFLiteModel, ModelRegistry |
| `Comms/` | `ESPNOWTransmitter.h` (test stub); production uses `esp_now_send` directly in main.cpp |
| `Filters/` | KalmanFilter1D, SlidingWindow, FeatureNormalizer (Madgwick not yet implemented) |

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
- **UDP Port 8888**: V5-historical (S3→PC direct UDP). Not active in V6; `CONFIG_UDP_PORT=8888` flag remains in platformio.ini but has no runtime impl. Current path is P4→USB CDC→relay.
- **Relay Host**: Web frontend connects to `ws://${relayHost}:8765` — default is `localhost`
- **File line endings**: Managed by `.gitattributes` — LF for all source, CRLF only for Windows scripts
- **P4 UART**: C6/S3→P4 uses 2Mbps UART with CRC-16/MODBUS. Frame: `[0xAA 0x55 69-byte payload CRC16_L CRC16_H]` = 73 bytes. P4 RX=GPIO38, TX=GPIO37.
- **P4 vs S3 builds**: P4/C6 use ESP-IDF (`idf.py`), S3 gloves use PlatformIO (`pio`) — different build systems
- **IMU = zeros (current)**: LSM6DSV16X driver not implemented yet. `SensorManager.readHardware()` zeroes quaternion/euler/gyro. BNO085 path removed from active code (lib_deps entry stale — see Tech Debt in README).

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