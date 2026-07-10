# EchoGlove V6.0 — Design Document Package

> **Date**: 2026-06-23 (updated 2026-07-10)
> **Supersedes**: V5.0 DualGloveFlex + V5.2 P4 Base Station
> **Key Changes**:
> - **IMU**: BNO085 ($15-25) → ST-LSM6DSV16X ($2-4)
> - **ADC**: 2× ADS1115 (¥8/glove) → **ESP32-S3 internal ADC1** (¥0) — see `07_internal_adc_migration.md`
>
> **Implementation status (code-verified 2026-07-10)**: This package is the **V6 design target**. In code: flex=internal ADC1 ✅, S3 comm=ESP-NOW ✅, P4 UART+USB CDC ✅. **Not yet in code**: LSM6DSV16X driver (IMU=zeros), wired UART S3→P4 (`WIRED_UART`). On-board C6 is ESP-Hosted (can't run ESP-NOW bridge) — dev path bypasses C6. See `PROGRESS.md` System Status.

## Document Index

| # | File | Lines | Content |
|---|------|-------|---------|
| 1 | `01_architecture_diagrams.md` | 1024 | System architecture, data flow, I2C topology, FreeRTOS tasks, 3-tier pipeline, communication stack, hot-switch, dual-hand features |
| 2 | `02_BOM_table.md` | 310 | Per-glove BOM, base station BOM, dev BOM, V5 vs V6 cost comparison, suppliers, breakout board options |
| 3 | `03_wiring_diagram.md` | 395 | LSM6DSV16X pinout, I2C bus, flex sensor circuit, C6↔P4 UART, P4 connections, BNO085→LSM6DSV16X migration |
| 4 | `04_SOP-SPEC-PLAN_V6.md` | 780 | **Main spec** — 12 sections: system overview, hardware, firmware, communication, relay, frontend, data/training, migration plan, references, risks, open questions, innovations |
| 5 | `05_claude_code_prompts.md` | 977 | 7 implementation phases with paste-ready Claude Code prompts (Chinese), file path reference, BNO085 vs LSM6DSV16X comparison. ⚠️ End-to-end flow diagram still shows legacy ESP-NOW→C6→P4 (deferred); treat as historical prompts — wired-UART path is the active dev target |
| 6 | `06_decision_summary.md` | 279 | 10 decisions logged, V5→V6 changes, cost analysis, risk assessment, compatibility matrix, future considerations, approval status |
| 7 | `07_internal_adc_migration.md` | NEW | **Internal ADC1 migration** — replaces 2× ADS1115: quantitative ENOB justification, IFlexSensor abstraction, InternalADCManager.h, NVS calibration, ADC1/ADC2 coexistence, validation plan, stale-fix list |

**Total**: ~4,500 lines (incl. 07)

## Quick Reference

- **IMU**: LSM6DSV16X @ 0x6A (SDO=GND), 6-axis, SFLP embedded fusion, $2-4 — **only I²C device**
- **ADC**: ESP32-S3 internal **ADC1** (GPIO1-5), `analogReadMilliVolts` + N=16 oversampling, NVS-calibrated — **no external ADC**
- **Flex**: 5× flex on 47kΩ divider → ADC1_CH0-4 (was ADS1115@0x48/0x49 in V5)
- **Feature Vector**: 11-dim single hand (unchanged), 28-dim dual hand (unchanged)
- **ESP-NOW Packet**: 69 bytes (unchanged)
- **SensorData Interface**: Identical to V5 — zero downstream changes
- **Cost Savings**: $26-42 (IMU) + ¥16/pair (ADC removal) per pair of gloves
- **Migration Effort**: ~400 lines new code (IMU), ~250 lines (ADC + IFlexSensor), ~30 lines changed

## Historical Versions

| Version | Location | Status |
|---------|----------|--------|
| V5.0 DualGloveFlex | `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md` | Superseded by V6 |
| V5.2 P4 Base Station | `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md` | Superseded by V6 |
| V5.2 Reference (GLM) | `docs/archive/v5-ai-drafts/V5.0DualGloveFlex/GLM_EchoGlove_V5.2_Document/` | Reference only (archived) |
| V5 AI Drafts | `docs/archive/v5-ai-drafts/` | Archive |
| V3 Hardware | `docs/archive/v3/` | Archive |
