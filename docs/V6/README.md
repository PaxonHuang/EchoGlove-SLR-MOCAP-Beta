# EchoGlove V6.0 — Design Document Package

> **Date**: 2026-06-23
> **Supersedes**: V5.0 DualGloveFlex + V5.2 P4 Base Station
> **Key Change**: BNO085 ($15-25) → ST-LSM6DSV16X ($2-4) IMU migration

## Document Index

| # | File | Lines | Content |
|---|------|-------|---------|
| 1 | `01_architecture_diagrams.md` | 1024 | System architecture, data flow, I2C topology, FreeRTOS tasks, 3-tier pipeline, communication stack, hot-switch, dual-hand features |
| 2 | `02_BOM_table.md` | 310 | Per-glove BOM, base station BOM, dev BOM, V5 vs V6 cost comparison, suppliers, breakout board options |
| 3 | `03_wiring_diagram.md` | 395 | LSM6DSV16X pinout, I2C bus, flex sensor circuit, ADS1115 channels, C6↔P4 UART, P4 connections, BNO085→LSM6DSV16X migration |
| 4 | `04_SOP-SPEC-PLAN_V6.md` | 780 | **Main spec** — 12 sections: system overview, hardware, firmware, communication, relay, frontend, data/training, migration plan, references, risks, open questions, innovations |
| 5 | `05_claude_code_prompts.md` | 977 | 7 implementation phases with paste-ready Claude Code prompts (Chinese), file path reference, BNO085 vs LSM6DSV16X comparison |
| 6 | `06_decision_summary.md` | 279 | 10 decisions logged, V5→V6 changes, cost analysis, risk assessment, compatibility matrix, future considerations, approval status |

**Total**: 3,765 lines

## Quick Reference

- **IMU**: LSM6DSV16X @ 0x6A (SDO=GND), 6-axis, SFLP embedded fusion, $2-4
- **Feature Vector**: 11-dim single hand (unchanged), 28-dim dual hand (unchanged)
- **ESP-NOW Packet**: 69 bytes (unchanged)
- **SensorData Interface**: Identical to V5 — zero downstream changes
- **Cost Savings**: $26-42 per pair of gloves
- **Migration Effort**: ~400 lines new code, ~30 lines changed

## Historical Versions

| Version | Location | Status |
|---------|----------|--------|
| V5.0 DualGloveFlex | `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md` | Superseded by V6 |
| V5.2 P4 Base Station | `docs/superpowers/specs/2026-06-10-v52-p4-base-station-design.md` | Superseded by V6 |
| V5.2 Reference (GLM) | `docs/V5.0DualGloveFlex/GLM_EchoGlove_V5.2_Document/` | Reference only |
| V5 AI Drafts | `docs/archive/v5-ai-drafts/` | Archive |
| V3 Hardware | `docs/archive/v3/` | Archive |
