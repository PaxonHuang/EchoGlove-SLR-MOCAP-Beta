# Phase Execution Skill
## Context
This is an ESP32-S3 hand sign recognition glove project (V6: flex sensors on internal ADC1 + LSM6DSV16X IMU). See CLAUDE.md for current hardware state. Phases:
- Phase 1: HAL layer + basic sensor drivers + data_structures
- Phase 2: Signal processing HAL (filtering, calibration)
- Phase 3: Gesture recognition algorithm

## Instructions
1. Read AGENTS.md and check existing project state first
2. Identify which phase the user wants to execute
3. Execute implementation immediately per specs — do NOT spend rounds exploring
4. After all file changes, run `pio run` to verify build
5. Summarize what was completed and what remains