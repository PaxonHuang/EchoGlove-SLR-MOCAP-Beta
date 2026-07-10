# PROGRESS.md — Cross-Session State Tracker

**Last verified vs code**: 2026-07-10
**Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x` (active)

> This is the authoritative progress file. `PROGRESS_CN.md` is a short Chinese summary. Historical debug details (V3 BNO085/TCA9548A/ADS1115 breadboard saga, 2026-05/06) are in `docs/archive/` and git history — not repeated here.

---

## System Status (code-verified 2026-07-10)

| Layer | Status | Detail |
|-------|--------|--------|
| ✅ Flex | Implemented | ESP32-S3 internal ADC1 (GPIO1–5), N=16 oversample, NVS calibration — `InternalADCManager.h` |
| ✅ S3 comm | Implemented | ESP-NOW broadcast (`esp_now_send`, 69B `GlovePacket`) |
| ✅ C6→P4 relay | Implemented (code) | `c6_firmware` ESP-NOW→UART 2Mbps. ⚠️ Cannot run on EV board's on-board C6 (ESP-Hosted) |
| ✅ P4 receive/output | Implemented | P4 UART RX (GPIO37/38, 2Mbps) → Tier2 stub → USB CDC JSON |
| ✅ P4 standalone | Verified | LVGL + TFLite stub + ES8311 audio + TinyUSB CDC (`CONFIG_P4_INTERNAL_MOCK=y`, commit `4f541bb`) |
| 🟡 IMU (LSM6DSV16X) | **Designed / not implemented** | Driver does not exist; IMU output = zeros (`SensorManager.h` TODO). BNO085 removed from active code |
| 🟡 Wired UART (S3→P4) | **Designed / not implemented** | `WIRED_UART` flag not in code; S3 uses ESP-NOW. Design: `specs/2026-07-08-s3-p4-wired-uart-design.md` |
| 🟡 Wi-Fi/UDP (future) | Planned | C6 ESP-Hosted → Wi-Fi/UDP → P4 (production) |
| ❌ BNO085 / ADS1115 | Deprecated | Dead code + stale lib_deps — see Tech Debt |

**Verified end-to-end path today**: P4 standalone with internal mock data only. (C6 ESP-Hosted can't run the bridge; wired UART not yet implemented.)

---

## V6.0 Migration — Flex internal ADC ✅, IMU/wired pending

**Spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md` · **ADC migration**: `docs/V6/07_internal_adc_migration.md`

### Completed
- **Flex internal ADC1 migration** (2026-07-02~03, TDD, 38/38 native tests pass, `pio run` SUCCESS):
  - `IFlexSensor.h` interface + `InternalADCManager.h` (GPIO1-5, N=16, Kalman, NVS `Preferences` "flex_cal") + `FlexManager.h` refactored to `IFlexSensor*` + `SensorManager.h` wiring + `main.cpp` V5 FreeRTOS restore (`SIMULATION=false`)
  - 6 commits `94bcfdb`→`7ed6624` (cleaned `1c6f4dc`), see git log
- **V6 design docs** (`docs/V6/01–07`) — LSM6DSV16X + internal ADC design complete
- **Dev environment** (2026-07-10): one-shot `/setup-env` + declarative conda envs (`environment.yml`/`environment_tf.yml`) + `docs/DEVELOPMENT_SETUP.md`

### Pending implementation
1. **LSM6DSV16X driver** (`lib/Sensors/LSM6DSV16XManager.h`) — SFLP quaternion + gyro; `MadgwickFilter.h` fallback. Wire into `SensorManager` (replace zero-IMU path)
2. **Wired UART** — `UARTTransmitter.h` + `WIRED_UART=1` parallel TX (TDD, plan `plans/2026-07-09-s3-p4-wired-uart.md`, phases 1–5 unwritten)
3. **On-device verification** V1–V7 (`07 §8`): boot-strap, ADC1↔ESP-NOW coexist, throughput<1ms, ENOB≥12, NVS, L1 accuracy, divider linearity — needs hardware
4. Optional: `ADS1115FlexAdapter.h` V5-compat adapter — descoped (not needed)

---

## Tech Debt (code, pending — to clear in a dedicated commit)

- `platformio.ini` `lib_deps`: `Adafruit BNO08x` + `NimBLE-Arduino` — no active source includes them (stale deps, safe to remove once diag envs reviewed)
- `glove_firmware/lib/Sensors/ADS1115Manager.h` — unreferenced dead code
- `lib/Sensors/Sensors.h` / `lib/Comms/Comms.h` aggregate-header comments — still describe V5 (ADS1115/BNO085)
- `data_structures.h` I2C comment "400kHz for 3 devices" — V5-stale (V6 single device, I2C not yet active)
- `ESPNOWTransmitter.h` — test stub only; production uses `esp_now_send` in main.cpp
- `CONFIG_UDP_PORT=8888` flag in platformio.ini — no runtime UDP impl (V5-historical)

---

## P4 Base Station — verified standalone (A3, 2026-07-08)

- **Board**: ESP32-P4-Function-EV-Board v1.5.2, `/dev/ttyACM0`, ESP-IDF v5.4, 32MB PSRAM
- **C6**: on-board ESP32-C6-MINI-1 = factory **ESP-Hosted** co-processor (SDIO, slave FW v0.0.6). Does NOT support ESP-NOW pass-through. NOT flashed. See memory `p4-ev-board-c6-esp-hosted`.
- **A3 result** (P4-only, `CONFIG_P4_INTERNAL_MOCK=y`): boot ✅, LVGL display ✅, FramePairer ✅, TFLite stub ✅, ES8311 audio init ✅, TinyUSB CDC init ✅. Commits `4f541bb`, `3cf2f3a`.
- **Track B** (mock relay→browser E2E): WS 403 fix (`/ws` endpoint), 0 JS errors, FPS=32 ✅
- Hardware debug history (TCA9548A/ADS1115 breadboard saga) → `docs/archive/`, superseded by V6 internal-ADC migration

---

## V5.3 Wired UART — S3→P4 Direct (2026-07-09, designed)

**Pivot**: on-board C6 is ESP-Hosted (no ESP-NOW). Dev path = S3 → P4 direct UART (bypass C6). C6 → future Wi-Fi.

- Tag `v5.3-wired-dev` (baseline). Design: `specs/2026-07-08-s3-p4-wired-uart-design.md`. Plan: `plans/2026-07-09-s3-p4-wired-uart.md`.
- S3 GPIO6 (TX, free) → P4 GPIO38 (RX) + GND. Dual-hand via dual UART (physical isolation). Framing: 73-byte CRC-16/MODBUS (`shared/uart_frame.h`).
- **Status**: designed only — Phase 1–5 (UARTTransmitter TDD → main.cpp parallel TX → P4 mock off → hardware verify → dual UART) all pending.

---

## V5.2 P4 Base Station — code-complete (historical)

- 8/8 tasks done (commits `f4e4d34`..`bef0c96`): UART framing (12/12 tests), C6 ESP-NOW→UART relay, P4 UART receive+FramePairer+28-dim, Tier2 TFLite stub, LVGL UI, TTS+USB CDC, PC relay USB CDC extension (88/88), integration tests.
- Spec/plan: `specs/2026-06-10-v52-p4-base-station-design.md`, `plans/2026-06-10-v52-p4-base-station.md` (historical — C6 ESP-NOW assumptions superseded by V5.3).

---

## Test Status (current)

| Component | Tests | Status |
|-----------|-------|--------|
| Firmware native (incl. V6 ADC suite) | 38 | ✅ |
| P4 base station native | 12 | ✅ |
| Relay pytest | (was 133; see `glove_relay/tests/`) | ✅ |
| Web | build | ✅ |

---

## MCP Plugins

Playwright ✅, Chrome DevTools ✅. Context7 / Espressif Docs intermittent (proxy `127.0.0.1`). GitHub MCP needs token in gitignored `.claude/settings.local.json`.

---

## Session Continuation Protocol

1. Read this file first
2. Check System Status table above — note 🟡/❌ items are NOT yet real
3. Continue from last checkpoint
4. Update this file when completing a task
