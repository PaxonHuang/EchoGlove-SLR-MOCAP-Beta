# EchoGlove V6.0 — Decision Summary & Log

> **Version**: V6.0
> **Date**: 2026-06-23
> **Status**: Pending user review
> **Supersedes**: V5.0 DualGloveFlex + V5.2 P4 Base Station
> **Branch**: V6-LSM6DSV16X (to be created from V5-DualGloveFlex)
> **Design Spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md`

---

## 1. Decision Log

| # | Date | Decision | Options Considered | Choice | Rationale |
|---|------|----------|-------------------|--------|-----------|
| D1 | 2026-06-23 | IMU sensor selection | BNO085 ($15-25), LSM6DSO16X ($4-6), **LSM6DSV16X ($2-4)**, BMI270 ($3-5), ICM-42688-P ($3-5) | **LSM6DSV16X** | Lowest cost ($2-4 vs BNO085 $15-25), embedded SFLP fusion, 0.65mA power, 0.8s calibration, ultra-low noise (70ug/rtHz accel, 3.8mdps/rtHz gyro), same 6-axis output, TRL9 production proven |
| D2 | 2026-06-23 | Magnetometer requirement | 9-axis (with mag), **6-axis (no mag)**, external mag add-on | **No magnetometer** | 6-axis SFLP fusion sufficient for hand posture tracking. Heading drift acceptable for gesture recognition (relative orientation matters, not absolute heading). Eliminates calibration complexity and BOM cost |
| D3 | 2026-06-23 | IMU count per glove | Single IMU, Dual IMU (wrist + fingertip) | **Single IMU** | Matches V5 architecture. One IMU on back of hand captures wrist orientation; flex sensors capture finger articulation. Dual IMU adds cost and complexity without proportional accuracy gain for 46-class gesture recognition |
| D4 | 2026-06-23 | Fusion computation location | PC-side fusion (send raw accel+gyro, fuse in relay), **MCU-side ready-made library**, Custom MCU fusion implementation | **MCU-side ready-made library** | ST official LSM6DSV16X driver + embedded SFLP hardware fusion. Zero MCU compute for quaternion output. Madgwick filter as MCU-side fallback if SFLP unavailable. Avoids PC dependency and latency |
| D5 | 2026-06-23 | SFLP vs Madgwick priority | **SFLP (hardware embedded)** as primary, Madgwick (software) as fallback | **SFLP primary** | SFLP runs on sensor's internal DSP, 0 MCU load, 0.8s calibration. Madgwick only if SFLP register access fails. Madgwick on ESP32-S3 @ 240MHz is ~0.05ms — viable fallback |
| D6 | 2026-06-23 | Optimization priority | Speed-only (minimize latency), Accuracy-only (maximize recognition), **Balanced/comprehensive** | **Balanced/comprehensive** | Project targets both real-time performance (<100ms E2E) and practical accuracy (>90% Tier1). Neither latency nor accuracy alone is sufficient for usable sign language translation |
| D7 | 2026-06-23 | I2C address assignment | LSM6DSV16X @ 0x6A (SDO=GND), alternative @ 0x6B (SDO=VDD) | **0x6A (SDO=GND)** | No conflict with ADS1115 @ 0x48/0x49. Flat bus, no MUX needed. SDO pin latched at power-up |
| D8 | 2026-06-23 | Migration strategy | Big-bang replace all, **New branch + simulation-first**, Incremental per-module | **New branch + simulation-first** | Create V6-LSM6DSV16X branch, validate end-to-end data flow before hardware commitment. SensorData interface unchanged means relay/frontend need zero changes |
| D9 | 2026-06-23 | Feature vector dimensions | Change to include quaternion directly, **Keep 11-dim (flex+euler+gyro)**, Expand to 14-dim | **Keep 11-dim unchanged** | Maintains full backward compatibility with existing models, relay, and frontend. Quaternion is intermediate; euler derivation is identical regardless of IMU source |
| D10 | 2026-06-23 | Backward compatibility scope | Full V5 compatibility, **Interface-compatible only**, Clean break | **Interface-compatible** | SensorData struct, GlovePacket (69B), feature vector (11/28-dim), ESP-NOW protocol all unchanged. Models may need retraining if IMU characteristics differ |

---

## 2. V5 to V6 Changes Summary

### 2.1 What Changed

| Component | V5 | V6 | Impact |
|-----------|----|----|--------|
| IMU sensor | BNO085 (SH2, Adafruit library) | LSM6DSV16X (ST driver + SFLP) | Hardware swap, new driver |
| IMU cost | $15-25 per unit | $2-4 per unit | **$13-21 savings per glove** |
| IMU power | ~15mA (BNO085 active) | 0.65mA (LSM6DSV16X combo) | **~95% power reduction for IMU** |
| IMU calibration | Manual (motion-based, ~10s) | SFLP auto (0.8s) | Faster startup, no user action |
| I2C address | 0x4B (BNO085) | 0x6A (LSM6DSV16X) | Address change, same bus |
| Firmware driver | Adafruit_BNO08x | LSM6DSV16XManager (new) | ~30 lines changed in SensorManager |
| platformio.ini | lib_deps: Adafruit_BNO08x | lib_deps: ST LSM6DSV16X driver | Dependency swap |
| Wiring | BNO085 10-pin | LSM6DSV16X 14-pin LGA | Different breakout, same GPIO8/9 |
| BOM per glove | ~$78 | ~$61.50 | **$16.50 savings** |

### 2.2 What Did NOT Change

| Component | Reason |
|-----------|--------|
| SensorData struct | Interface identical — quaternion[4], euler[3], gyro[3], flex[5] |
| GlovePacket (69 bytes) | Same wire format, same ESP-NOW broadcast |
| Feature vector (11-dim per hand) | flex[5] + euler[3] + gyro[3] — unchanged |
| Dual-hand features (28-dim) | Left[11] + Right[11] + Relative[6] — unchanged |
| FreeRTOS task architecture | Same 4 tasks, same core assignment, same priorities |
| ADS1115 drivers | No change to ADC or flex sensor handling |
| Flex sensor hardware | Same SpectraFlex 2.2", same 47k pull-down |
| ESP-NOW protocol | Same broadcast, same packet format, same timing |
| UART protocol (C6 to P4) | Same 2Mbps, same CRC-16 frame format |
| Relay server (Python FastAPI) | Zero code changes needed |
| Frontend (React3F / Unity) | Zero code changes needed |
| Tier1/Tier2/Tier3 models | Architecture unchanged; may need retraining on new data |
| P4 base station firmware | Zero changes — receives same 28-dim features |
| C6 relay firmware | Zero changes — forwards same 69-byte packets |
| Hot-switch / blend logic | Same 5-frame linear blend, same tier fallback |
| NLP pipeline | Zero changes |
| TTS pipeline | Zero changes |

---

## 3. Cost Impact Analysis

### 3.1 Per-Glove BOM

| Component | V5 Cost | V6 Cost | Delta |
|-----------|---------|---------|-------|
| ESP32-S3-DevKitC-1 N16R8 | $4.50 | $4.50 | $0 |
| IMU (BNO085 → LSM6DSV16X) | $18.00 | $3.00 | **-$15.00** |
| ADS1115 x2 | $1.60 | $1.60 | $0 |
| Flex sensor x5 | $2.25 | $2.25 | $0 |
| 47k resistors x5 | $0.05 | $0.05 | $0 |
| Breakout board / PCB | $1.00 | $1.50 | +$0.50 |
| Wiring / connectors | $0.75 | $0.75 | $0 |
| **Total per glove** | **$28.15** | **$13.65** | **-$14.50** |

### 3.2 Per-Pair and Scaled Cost

| Quantity | V5 Total | V6 Total | Savings |
|----------|----------|----------|---------|
| 1 pair (2 gloves) | $56.30 | $27.30 | **$29.00 (51.5%)** |
| 5 pairs (10 gloves) | $281.50 | $136.50 | **$145.00** |
| 10 pairs (20 gloves) | $563.00 | $273.00 | **$290.00** |
| Production batch (100 units) | $2,815.00 | $1,365.00 | **$1,450.00** |

### 3.3 Non-Recurring Engineering (NRE)

| Item | Cost | Notes |
|------|------|-------|
| LSM6DSV16X breakout boards | $10-20 | 2-3 boards for dev + spare |
| Driver development time | ~3 days | LSM6DSV16XManager + Madgwick fallback |
| Model retraining time | ~5 days | If IMU characteristics require new dataset |
| **Total NRE** | **~8 days + $20** | One-time cost, amortized across all units |

### 3.4 Break-Even Analysis

- NRE cost (8 days labor) is recouped after producing **~3 pairs** at V5 pricing delta
- For any production quantity > 3 units, V6 is strictly cheaper

---

## 4. Risk Assessment

| # | Risk | Probability | Impact | Severity | Mitigation | Owner |
|---|------|-------------|--------|----------|------------|-------|
| R1 | SFLP accuracy worse than BNO085 for gesture classification | Low | High | Medium | SFLP heading 0.5/5min, pitch/roll 1.5 static — comparable to BNO085. Madgwick fallback available. Retrain models if accuracy drops >3% | Firmware |
| R2 | LSM6DSV16X breakout board unavailable / long lead time | Low | High | Medium | Order early (Phase 0). LGA-14L has commercial breakouts from Adafruit, SparkFun, AliExpress. Worst case: design custom PCB | Hardware |
| R3 | SFLP register access differs from datasheet (silicon errata) | Low | Medium | Low | ST official C driver abstracts register access. AN5763 application note covers known issues. Madgwick fallback if SFLP blocked | Firmware |
| R4 | Model accuracy regression after IMU swap | Medium | High | High | Feature vector interface unchanged, but IMU noise characteristics differ. Collect new training data with LSM6DSV16X. Budget 5 days for retraining | ML |
| R5 | Power-on timing / initialization race condition | Low | Medium | Low | SFLP stabilization is 0.7s. Add explicit delay in SensorManager::begin() before first read. Watchdog monitors init success | Firmware |
| R6 | I2C bus stability with new device at 400kHz | Low | Low | Low | Same bus, same speed, same pull-ups. LSM6DSV16X supports up to 1MHz I2C. No address conflict (0x6A vs 0x48/0x49) | Firmware |
| R7 | Flex sensor temperature drift compounds with IMU change | Medium | Low | Low | LSM6DSV16X has built-in temperature sensor. Use for flex sensor drift compensation (0.5%/C). Separate concern from IMU migration | Firmware |
| R8 | SFLP quaternion convention differs from BNO085 | Medium | Medium | Medium | BNO085 uses Android convention (x,y,z,w). LSM6DSV16X SFLP uses Q14 format. Verify quaternion-to-Euler conversion produces matching output ranges. Unit test quaternion normalization | Firmware |

### Risk Summary

- **No blocking risks** — all mitigations are implementable within the migration timeline
- **Highest risk (R4)**: Model retraining — budget 5 days, start data collection in Phase 2
- **Contingency**: If SFLP fails entirely, Madgwick filter on ESP32-S3 adds only 0.05ms latency

---

## 5. Compatibility Matrix

### 5.1 Full Backward Compatibility (Zero Changes)

| Component | Compatible | Notes |
|-----------|-----------|-------|
| SensorData struct | YES | Identical fields: flex[5], quaternion[4], euler[3], gyro[3] |
| GlovePacket (69 bytes) | YES | Same wire format, same ESP-NOW broadcast |
| Feature vector (11-dim) | YES | flex[5] + euler[3] + gyro[3] |
| Dual-hand features (28-dim) | YES | Left[11] + Right[11] + Relative[6] |
| ESP-NOW protocol | YES | Same broadcast mode, same packet size |
| C6 relay firmware | YES | Forwards 69-byte packets, no parsing change |
| P4 base station firmware | YES | Receives same 28-dim features, same UART protocol |
| Relay server (Python) | YES | Receives same JSON/Protobuf, same WebSocket output |
| React3F frontend | YES | Same WebSocket data format |
| Unity XR frontend | YES | Same WebSocket data format |
| Kalman filter | YES | Same 11-channel input, same filter parameters |
| Sliding window | YES | Same 30-frame ring buffer |
| Hot-switch / blend logic | YES | Same tier selection, same 5-frame blend |
| NLP pipeline | YES | Same gesture ID input |
| TTS pipeline | YES | Same text input |

### 5.2 Requires Modification

| Component | Change Scope | Effort |
|-----------|-------------|--------|
| SensorManager.cpp | Replace BNO085 API calls with LSM6DSV16X calls (~30 lines) | 1 day |
| platformio.ini | Swap lib_deps: remove Adafruit_BNO08x, add ST driver | 10 min |
| I2C scan / debug output | Update expected address from 0x4B to 0x6A | 10 min |
| Wiring documentation | Update pin diagrams for LSM6DSV16X | 30 min |

### 5.3 May Require Retraining

| Model | Retrain Needed | Reason |
|-------|---------------|--------|
| Tier1 (CNN+SE-Attention) | Likely yes | IMU noise profile and dynamic range differ between BNO085 and LSM6DSV16X |
| Tier2 (Gated Bi-CrossAttn) | Likely yes | Same reason — relative features depend on IMU characteristics |
| Tier3-L1 (Gated Bi-CrossAttn + MS-TCN) | Likely yes | Same reason |
| Tier3-L2 (ST-GCN + MS-TCN + CTC) | Likely yes | Same reason |

**Key insight**: The feature vector *format* is identical, but the *distribution* of values may shift due to different IMU noise, scale, and fusion behavior. Retraining ensures the models learn the new sensor characteristics.

### 5.4 Compatibility Verdict

| Layer | Status |
|-------|--------|
| Hardware | **Breaking** — new sensor, new wiring, new breakout |
| Firmware | **Minor change** — ~30 lines in SensorManager + new driver module |
| Communication | **Fully compatible** — zero changes |
| Relay | **Fully compatible** — zero changes |
| Frontend | **Fully compatible** — zero changes |
| Models | **Retrain required** — same architecture, new data |

---

## 6. Future Considerations

### 6.1 MLC (Machine Learning Core)

| Aspect | Detail |
|--------|--------|
| What | LSM6DSV16X has an embedded MLC block supporting up to 16 decision tree classes |
| Potential use | Pre-classify simple gestures (open/closed fist, pointing) directly on sensor, reducing MCU load |
| Decision tree | ST provides pre-trained decision trees for common use cases; custom trees via Unico GUI |
| V6 status | **Not used in V6.0** — Tier1 CNN on ESP32-S3 handles all 46 classes |
| Future path | V6.1+: offload binary gesture detection to MLC (e.g., "is hand open?"), wake MCU only on gesture detection for power saving |

### 6.2 FSM (Finite State Machine)

| Aspect | Detail |
|--------|--------|
| What | Programmable FSM block on LSM6DSV16X for interrupt-driven state transitions |
| Potential use | Detect motion patterns (tap, tilt, rotation) without MCU polling |
| V6 status | **Not used in V6.0** — continuous 100Hz polling is required for gesture recognition |
| Future path | V6.1+: use FSM for wake-on-motion to implement sleep mode between gestures |

### 6.3 Qvar (Electrode Detection)

| Aspect | Detail |
|--------|--------|
| What | LSM6DSV16X has a Qvar channel for capacitive/proximity sensing via electrode |
| Potential use | Detect hand presence in glove, or proximity-based activation |
| V6 status | **Not used in V6.0** — flex sensors already detect hand state |
| Future path | V7+: integrate Qvar electrode on glove palm for automatic wear detection and gesture triggering |

### 6.4 Magnetometer Addition

| Aspect | Detail |
|--------|--------|
| What | External 3-axis magnetometer (e.g., LIS2MDL, $1-2) for 9-axis fusion |
| Potential use | Absolute heading reference, eliminates yaw drift over long sessions |
| V6 status | **Not needed** — 6-axis SFLP is sufficient for gesture recognition (relative orientation matters, not absolute heading) |
| Future path | If MOCAP use case requires absolute orientation (e.g., VR world-locked hand tracking), add LIS2MDL on same I2C bus (address 0x1E). Would change feature vector to 14-dim per hand (quaternion[4] + euler[3] + gyro[3] + mag[3] + flex[5] excluded from mag) |

### 6.5 Custom PCB

| Aspect | Detail |
|--------|--------|
| Current | Breadboard + breakout boards for development |
| Future | Custom PCB integrating ESP32-S3 module + LSM6DSV16X + 2x ADS1115 + flex connectors |
| Benefit | Reduced wiring, smaller form factor, production-ready |
| Timeline | Post-competition, when design is frozen |

### 6.6 Multi-Gesture Continuous Recognition

| Aspect | Detail |
|--------|--------|
| Current | 46 isolated gesture classes, Phase 1 |
| Future | Continuous sign language sentence decoding via CTC beam search |
| Impact | Same sensor pipeline, but model architecture changes (ST-GCN + MS-TCN + CTC) |
| V6 readiness | Feature vector and data pipeline already support sliding window (30 frames, 50% overlap) |

---

## 7. Approval Status

| Item | Status | Reviewer | Date | Notes |
|------|--------|----------|------|-------|
| IMU selection (LSM6DSV16X) | **Pending** | User | - | Awaiting review |
| No magnetometer decision | **Pending** | User | - | Awaiting review |
| Single IMU per glove | **Pending** | User | - | Awaiting review |
| MCU-side fusion (SFLP + Madgwick) | **Pending** | User | - | Awaiting review |
| Backward compatibility scope | **Pending** | User | - | Awaiting review |
| Cost model and BOM | **Pending** | User | - | Awaiting review |
| Risk mitigations | **Pending** | User | - | Awaiting review |
| Migration plan (17 days) | **Pending** | User | - | Awaiting review |
| **Overall V6 design** | **PENDING** | User | - | Document set: 04 + 06 |

### Sign-Off

```
Reviewer: _________________________
Date:     _________________________
Decision: [ ] Approved  [ ] Approved with changes  [ ] Rejected
Notes:    _________________________
```

---

## Appendix: Decision Timeline

| Date | Event | Participants |
|------|-------|-------------|
| 2026-06-23 | V6 brainstorming: IMU candidate evaluation | User + Claude |
| 2026-06-23 | Decision D1-D6 confirmed (IMU, mag, count, fusion, priority) | User + Claude |
| 2026-06-23 | Decision D7-D10 confirmed (address, migration, features, compat) | User + Claude |
| 2026-06-23 | Design spec drafted (04_SOP-SPEC-PLAN_V6.md) | Claude |
| 2026-06-23 | Decision summary drafted (this document) | Claude |
| TBD | User review and approval | User |
| TBD | Phase 0: Branch creation and setup | - |
